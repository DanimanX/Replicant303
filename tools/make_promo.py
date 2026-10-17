#!/usr/bin/env python3
"""
Replicant303 - hard techno / acid promo generator.

Builds the full soundtrack (acid stem from the plugin + synthesized techno bed)
and renders a 1920x1080 / 30 fps Blade-Runner-flavoured promo in English using
the real panel screenshots produced by generate_snapshot.

Usage:
    python3 tools/make_promo.py [--audio-only] [--frames-only]

Outputs:
    assets/replicant303_promo_hardtechno_1080p.mp4
    assets/replicant303_promo_hardtechno_poster.png
"""

import math
import os
import subprocess
import sys
import wave

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from scipy import signal

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets")

W, H = 1920, 1080
FPS = 30
DURATION = 64.0

AMBER = (255, 184, 92)
AMBER_HOT = (255, 224, 170)
CYAN = (92, 226, 255)
RED = (255, 54, 66)
WHITE = (232, 240, 238)
MUTED = (150, 168, 168)
DARK = (4, 6, 9)

FONT_TITLE = "/usr/share/fonts/TTF/RobotoCondensed-Bold.ttf"
FONT_MONO = "/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Bold.ttf"
FONT_MONO_REG = "/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Regular.ttf"

_cache = {}


def font(path, size):
    key = (path, size)
    if key not in _cache:
        _cache[key] = ImageFont.truetype(path, size)
    return _cache[key]


# ---------------------------------------------------------------------------
# Audio
# ---------------------------------------------------------------------------
def load_acid(path, n):
    with wave.open(path, "rb") as w:
        sr = w.getframerate()
        ch = w.getnchannels()
        sw = w.getsampwidth()
        raw = np.frombuffer(w.readframes(w.getnframes()), dtype=np.uint8)

    if sw == 3:
        b = raw.reshape(-1, 3)
        v = (b[:, 0].astype(np.int32)
             | (b[:, 1].astype(np.int32) << 8)
             | (b[:, 2].astype(np.int32) << 16))
        v = np.where(v >= 1 << 23, v - (1 << 24), v).astype(np.float32) / 8388608.0
    elif sw == 2:
        v = raw.view(np.int16).astype(np.float32) / 32768.0
    else:
        v = raw.astype(np.float32) / 127.0 - 1.0

    v = v.reshape(-1, ch)
    if ch == 1:
        v = np.repeat(v, 2, axis=1)

    if len(v) < n:
        v = np.pad(v, ((0, n - len(v)), (0, 0)))
    return v[:n]


def env_ramp(n, points):
    """points: [(time_seconds, gain), ...] -> array of length n."""
    times = np.array([p[0] for p in points], dtype=np.float64)
    gains = np.array([p[1] for p in points], dtype=np.float64)
    t = np.arange(n) / SAMPLE_RATE
    return np.interp(t, times, gains).astype(np.float32)


SAMPLE_RATE = 48000


def make_kick(rng):
    n = int(0.55 * SAMPLE_RATE)
    t = np.arange(n) / SAMPLE_RATE
    f = 46.0 + 150.0 * np.exp(-t * 34.0)
    phase = 2.0 * np.pi * np.cumsum(f) / SAMPLE_RATE
    body = np.sin(phase) * np.exp(-t * 6.2)
    click = rng.standard_normal(n) * np.exp(-t * 110.0) * 0.30
    x = np.tanh((body + click) * 1.7)
    return (x / max(np.abs(x).max(), 1e-9) * 0.92).astype(np.float32)


def make_hat(rng, dur, decay, hp=7000.0):
    n = int(dur * SAMPLE_RATE)
    t = np.arange(n) / SAMPLE_RATE
    noise = rng.standard_normal(n)
    sos = signal.butter(2, hp, "hp", fs=SAMPLE_RATE, output="sos")
    noise = signal.sosfilt(sos, noise)
    x = noise * np.exp(-t * decay)
    return (x / max(np.abs(x).max(), 1e-9) * 0.8).astype(np.float32)


def make_clap(rng):
    n = int(0.32 * SAMPLE_RATE)
    t = np.arange(n) / SAMPLE_RATE
    sos = signal.butter(2, [900.0, 3200.0], "bp", fs=SAMPLE_RATE, output="sos")
    noise = signal.sosfilt(sos, rng.standard_normal(n))
    env = np.exp(-t * 11.0)
    for delay, amp in ((0.0, 1.0), (0.011, 0.8), (0.023, 0.6)):
        env += amp * np.exp(-np.maximum(t - delay, 0.0) * 55.0) * (t >= delay)
    x = noise * env
    return (x / max(np.abs(x).max(), 1e-9) * 0.8).astype(np.float32)


def build_audio():
    n = int(DURATION * SAMPLE_RATE)
    t = np.arange(n) / SAMPLE_RATE
    rng = np.random.default_rng(303)

    acid_path = os.path.join(ASSETS, "replicant303_hardtechno_demo.wav")
    if not os.path.exists(acid_path):
        raise SystemExit(
            "Missing assets/replicant303_hardtechno_demo.wav\n"
            "Render it first:\n"
            "  ninja -C build render_audio_demo\n"
            "  ./build/render_audio_demo assets/replicant303_hardtechno_demo.wav"
        )

    acid = load_acid(acid_path, n)

    beat = 60.0 / 138.0
    first_kick = 14 * beat

    kick = np.zeros(n, dtype=np.float32)
    kick_hit = make_kick(rng)
    k = 0
    while first_kick + k * beat < 58.0:
        pos = int((first_kick + k * beat) * SAMPLE_RATE)
        end = min(pos + len(kick_hit), n)
        kick[pos:end] += kick_hit[: end - pos]
        k += 1

    # 16th hats and offbeat open hats from bar 8.
    hats = np.zeros(n, dtype=np.float32)
    hat_closed = make_hat(rng, 0.075, 85.0)
    hat_open = make_hat(rng, 0.20, 20.0, hp=6000.0)
    step = beat / 4.0
    s = 0
    while first_kick + s * step < 57.0:
        pos = int((first_kick + s * step) * SAMPLE_RATE)
        if first_kick + s * step > 22.0:
            hit = hat_open if (s % 4 == 2) else hat_closed
            amp = 0.45 if (s % 4 == 2) else (0.30 if s % 2 else 0.16)
            end = min(pos + len(hit), n)
            hats[pos:end] += hit[: end - pos] * amp
        s += 1

    clap = np.zeros(n, dtype=np.float32)
    clap_hit = make_clap(rng)
    b = 0
    while first_kick + b * beat < 56.0:
        if b % 4 in (1, 3) and first_kick + b * beat > 23.0:
            pos = int((first_kick + b * beat) * SAMPLE_RATE)
            end = min(pos + len(clap_hit), n)
            clap[pos:end] += clap_hit[: end - pos] * 0.6
        b += 1

    # Cinematic A-minor drone, detuned and low-passed.
    pad = np.zeros((n, 2), dtype=np.float32)
    for ch, detune in ((0, 0.998), (1, 1.002)):
        p = np.zeros(n, dtype=np.float32)
        for f in (55.0, 82.407, 110.0, 130.813, 164.814):
            p += (2.0 * ((f * detune * t) % 1.0) - 1.0).astype(np.float32) * 0.16
        sos = signal.butter(2, 420.0, "lp", fs=SAMPLE_RATE, output="sos")
        p = signal.sosfilt(sos, p).astype(np.float32)
        pad[:, ch] = p

    pad_gain = env_ramp(n, [(0, 0.55), (6, 0.30), (24, 0.18), (40, 0.20),
                            (56, 0.30), (62, 0.35), (64, 0.0)])

    # Rain / air bed.
    rain = np.zeros((n, 2), dtype=np.float32)
    for ch in range(2):
        noise = rng.standard_normal(n).astype(np.float32)
        sos = signal.butter(2, 1800.0, "hp", fs=SAMPLE_RATE, output="sos")
        noise = signal.sosfilt(sos, noise).astype(np.float32)
        slow = 0.7 + 0.3 * np.sin(2 * np.pi * t / 17.0 + ch)
        rain[:, ch] = noise * slow
    rain *= 0.035

    # Sidechain pump from the kick.
    duck = np.ones(n, dtype=np.float32)
    k = 0
    while first_kick + k * beat < 58.0:
        pos = int((first_kick + k * beat) * SAMPLE_RATE)
        length = int(0.30 * SAMPLE_RATE)
        end = min(pos + length, n)
        e = np.exp(-np.arange(end - pos) / (0.085 * SAMPLE_RATE))
        duck[pos:end] = np.minimum(duck[pos:end], (1.0 - 0.62 * e).astype(np.float32))
        k += 1

    # Reverse swell into the 40 s drop.
    swell = np.zeros(n, dtype=np.float32)
    swell_start = int(35.0 * SAMPLE_RATE)
    swell_end = int(40.0 * SAMPLE_RATE)
    seg_n = swell_end - swell_start
    if seg_n > 0:
        noise = rng.standard_normal(seg_n)
        sos = signal.butter(2, 2500.0, "hp", fs=SAMPLE_RATE, output="sos")
        noise = signal.sosfilt(sos, noise)
        ramp = (np.arange(seg_n) / seg_n) ** 2.2
        swell[swell_start:swell_end] = (noise * ramp).astype(np.float32) * 0.7

    acid_gain = env_ramp(n, [(0, 0.0), (2.0, 0.32), (6.0, 0.85), (56.0, 0.85),
                             (60.0, 0.35), (64.0, 0.0)])

    mix = (acid * acid_gain[:, None] * duck[:, None] * 0.85
           + kick[:, None] * 0.85
           + hats[:, None] * 0.55
           + clap[:, None] * 0.65
           + pad * pad_gain[:, None] * 0.8
           + rain * env_ramp(n, [(0, 1.0), (10, 0.25), (54, 0.25), (60, 0.7), (64, 0.0)])[:, None]
           + swell[:, None] * 0.45)

    # Gentle glue.
    mix = np.tanh(mix * 1.15) / np.tanh(1.15)
    peak = np.abs(mix).max()
    if peak > 0:
        mix *= 0.80 / peak

    fade = int(0.6 * SAMPLE_RATE)
    mix[:fade] *= np.linspace(0.0, 1.0, fade)[:, None]
    mix[-int(2.0 * SAMPLE_RATE):] *= np.linspace(1.0, 0.0, int(2.0 * SAMPLE_RATE))[:, None]

    out = os.path.join(ASSETS, "replicant303_hardtechno_mix.wav")
    pcm = (np.clip(mix, -1.0, 1.0) * 32767.0).astype(np.int16)
    with wave.open(out, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SAMPLE_RATE)
        w.writeframes(pcm.tobytes())

    print(f"[audio] wrote {out}  ({DURATION:.0f}s, peak {np.abs(mix).max():.2f})")
    return out


# ---------------------------------------------------------------------------
# Visual helpers
# ---------------------------------------------------------------------------
def spaced_text(draw, xy, text, fnt, fill, spacing=0.0, anchor="la"):
    widths = [draw.textlength(c, font=fnt) for c in text]
    total = sum(widths) + spacing * max(len(text) - 1, 0)
    x, y = xy
    if anchor[0] == "m":
        x -= total / 2.0
    elif anchor[0] == "r":
        x -= total
    for c, w in zip(text, widths):
        draw.text((x, y), c, font=fnt, fill=fill)
        x += w + spacing
    return total


def spaced_width(draw, text, fnt, spacing=0.0):
    widths = [draw.textlength(c, font=fnt) for c in text]
    return sum(widths) + spacing * max(len(text) - 1, 0)


def glow_layer(size, draw_fn, colour, blur):
    layer = Image.new("RGBA", size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    draw_fn(d, colour)
    return layer.filter(ImageFilter.GaussianBlur(blur))


def make_city():
    rng = np.random.default_rng(7)
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)

    # Sky glow behind the skyline.
    sky = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sky)
    for i in range(26):
        y = 700 + i * 9
        col = (255, 150, 70, max(0, 26 - i))
        sd.rectangle([0, y, W, y + 9], fill=col)
    layer = Image.alpha_composite(layer, sky.filter(ImageFilter.GaussianBlur(28)))
    d = ImageDraw.Draw(layer)

    # Far skyline.
    x = -40
    while x < W + 40:
        bw = int(rng.integers(50, 150))
        bh = int(rng.integers(160, 420))
        top = H - 180 - bh
        shade = int(rng.integers(9, 18))
        d.rectangle([x, top, x + bw, H], fill=(shade, shade + 5, shade + 9, 255))
        if rng.random() < 0.35:
            ax = x + bw // 2
            d.line([(ax, top), (ax, top - int(rng.integers(30, 90)))], fill=(20, 24, 28, 255), width=2)
        x += bw + int(rng.integers(6, 24))

    # Near skyline.
    x = -80
    while x < W + 80:
        bw = int(rng.integers(90, 240))
        bh = int(rng.integers(220, 560))
        top = H - 40 - bh
        shade = int(rng.integers(4, 11))
        d.rectangle([x, top, x + bw, H], fill=(shade, shade + 3, shade + 6, 255))
        # Windows.
        cols = max(2, bw // 22)
        rows = max(3, bh // 26)
        for c in range(cols):
            for r in range(rows):
                if rng.random() < 0.14:
                    wx = x + 8 + c * 22
                    wy = top + 14 + r * 26
                    if wx + 4 < x + bw and wy + 6 < H:
                        col = AMBER if rng.random() < 0.7 else CYAN
                        a = int(rng.integers(25, 110))
                        d.rectangle([wx, wy, wx + 5, wy + 7], fill=col + (a,))
        x += bw + int(rng.integers(4, 18))

    return layer


def make_fog():
    rng = np.random.default_rng(11)
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for _ in range(18):
        x = int(rng.integers(-200, W + 200))
        y = int(rng.integers(520, 1000))
        rx = int(rng.integers(180, 520))
        ry = int(rng.integers(40, 130))
        col = AMBER + (int(rng.integers(5, 16)),) if rng.random() < 0.5 else (80, 150, 170, int(rng.integers(5, 14)))
        d.ellipse([x - rx, y - ry, x + rx, y + ry], fill=col)
    return layer.filter(ImageFilter.GaussianBlur(70))


def make_vignette():
    y, x = np.ogrid[:H, :W]
    cx, cy = W / 2, H / 2
    r = np.sqrt(((x - cx) / (W * 0.62)) ** 2 + ((y - cy) / (H * 0.62)) ** 2)
    alpha = np.clip((r - 0.55) * 220, 0, 190).astype(np.uint8)
    arr = np.zeros((H, W, 4), dtype=np.uint8)
    arr[..., 3] = alpha
    return Image.fromarray(arr, "RGBA")


def make_scanlines():
    arr = np.zeros((H, W, 4), dtype=np.uint8)
    arr[::3, :, 3] = 34
    arr[1::3, :, 3] = 12
    return Image.fromarray(arr, "RGBA")


def make_noise_tiles(count=5):
    rng = np.random.default_rng(23)
    tiles = []
    for _ in range(count):
        small = rng.integers(0, 28, size=(H // 4, W // 4), dtype=np.uint8)
        arr = np.zeros((H, W, 4), dtype=np.uint8)
        arr[..., 3] = np.repeat(np.repeat(small, 4, axis=0), 4, axis=1)
        tiles.append(Image.fromarray(arr, "RGBA"))
    return tiles


def gradient_background(top=(7, 12, 18), bottom=(1, 2, 4)):
    img = Image.new("RGB", (W, H), bottom)
    px = img.load()
    for y in range(H):
        k = y / (H - 1)
        c = tuple(int(top[i] * (1 - k) + bottom[i] * k) for i in range(3))
        for x in range(W):
            px[x, y] = c
    return img


def draw_rain(img, t, intensity=1.0):
    rng = np.random.default_rng(int(t * 1000) + 5)
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    count = int(260 * intensity)
    for _ in range(count):
        x = int(rng.integers(-100, W + 100))
        y = int(rng.integers(0, H))
        length = int(rng.integers(18, 70))
        dx = int(length * 0.16)
        a = int(rng.integers(14, 55))
        col = (170, 200, 215, a) if rng.random() < 0.8 else AMBER + (a,)
        d.line([(x, y), (x + dx, y + length)], fill=col, width=1)
    img.alpha_composite(layer)


def draw_ui(frame, src, rect, accent=AMBER, scan_t=None):
    """Paste a panel screenshot with a neon frame, glow and optional scan line."""
    x, y, w, h = rect
    shot = src.resize((w, h), Image.LANCZOS)

    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.rectangle([x - 14, y - 14, x + w + 14, y + h + 14], outline=accent + (90,), width=10)
    frame.alpha_composite(glow.filter(ImageFilter.GaussianBlur(22)))

    shadow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    sd = ImageDraw.Draw(shadow)
    sd.rectangle([x + 10, y + 16, x + w + 10, y + h + 16], fill=(0, 0, 0, 170))
    frame.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(18)))

    frame.alpha_composite(shot.convert("RGBA"), (x, y))

    d = ImageDraw.Draw(frame)
    d.rectangle([x - 2, y - 2, x + w + 2, y + h + 2], outline=accent + (210,), width=2)

    # Corner ticks.
    tick = 26
    for cx, cy, sx, sy in ((x - 8, y - 8, 1, 1), (x + w + 8, y - 8, -1, 1),
                           (x - 8, y + h + 8, 1, -1), (x + w + 8, y + h + 8, -1, -1)):
        d.line([(cx, cy), (cx + sx * tick, cy)], fill=CYAN + (200,), width=2)
        d.line([(cx, cy), (cx, cy + sy * tick)], fill=CYAN + (200,), width=2)

    if scan_t is not None:
        sy = y + int((scan_t % 1.0) * h)
        sl = Image.new("RGBA", (W, 40), (0, 0, 0, 0))
        sld = ImageDraw.Draw(sl)
        sld.rectangle([x, 18, x + w, 22], fill=AMBER + (60,))
        frame.alpha_composite(sl.filter(ImageFilter.GaussianBlur(6)), (0, sy - 20))


def caption(frame, x, y, kicker, title, sub=None, colour=AMBER):
    d = ImageDraw.Draw(frame)
    d.line([(x, y - 16), (x + 70, y - 16)], fill=colour + (255,), width=3)
    d.text((x, y), kicker, font=font(FONT_MONO, 22), fill=colour + (255,))
    d.text((x, y + 34), title, font=font(FONT_TITLE, 58), fill=WHITE + (255,))
    if sub:
        d.text((x, y + 104), sub, font=font(FONT_MONO_REG, 24), fill=MUTED + (255,))


def big_title(frame, text, y, size=190, colour=AMBER, spacing=6.0, alpha=255, glitch=0.0):
    d0 = ImageDraw.Draw(frame)
    fnt = font(FONT_TITLE, size)
    total = spaced_width(d0, text, fnt, spacing)
    x = (W - total) / 2.0

    # Glow.
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    spaced_text(gd, (x, y), text, fnt, colour + (255,), spacing)
    frame.alpha_composite(glow.filter(ImageFilter.GaussianBlur(26)))

    # Chromatic split.
    layer_r = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    dr = ImageDraw.Draw(layer_r)
    spaced_text(dr, (x - 3 - glitch, y), text, fnt, (255, 30, 60, 120), spacing)
    frame.alpha_composite(layer_r)

    layer_c = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    dc = ImageDraw.Draw(layer_c)
    spaced_text(dc, (x + 3 + glitch, y), text, fnt, (40, 220, 255, 110), spacing)
    frame.alpha_composite(layer_c)

    d = ImageDraw.Draw(frame)
    spaced_text(d, (x, y), text, fnt, colour + (alpha,), spacing)


def kick_pulse(t):
    beat = 60.0 / 138.0
    first = 14 * beat
    if t < first:
        return 0.0
    phase = ((t - first) % beat) / beat
    return float(max(0.0, 1.0 - phase * 3.2))


# ---------------------------------------------------------------------------
# Scenes
# ---------------------------------------------------------------------------
def scene_intro(frame, t):
    d = ImageDraw.Draw(frame)
    d.text((120, 96), "OFF-WORLD AUDIO DIVISION // SIGNAL 303", font=font(FONT_MONO, 24), fill=CYAN + (220,))
    d.text((W - 300, 96), "LOS ANGELES  //  2049", font=font(FONT_MONO, 24), fill=MUTED + (180,))

    lines = [
        (0.6, "> BOOTING REPLICANT CORE .............. OK"),
        (1.6, "> PATTERN BANK ....................... 8 x 16"),
        (2.6, "> ACID PROTOCOL ...................... ENGAGED"),
        (3.6, "> HUMAN INPUT ........................ OPTIONAL"),
        (4.6, "> SIGNAL ACQUIRED"),
    ]

    y = 330
    for at, line in lines:
        if t < at:
            break
        shown = line[: max(0, int((t - at) * 40) + 1)]
        d.text((150, y), shown, font=font(FONT_MONO_REG, 28), fill=AMBER + (235,))
        y += 52

    if int(t * 2.5) % 2 == 0:
        d.rectangle([150, y + 2, 168, y + 30], fill=AMBER + (220,))

    # Thin line that grows.
    width = int(min(1.0, t / 5.0) * 620)
    d.line([(150, 290), (150 + width, 290)], fill=AMBER + (190,), width=2)


def scene_title(frame, t, local):
    big_title(frame, "REPLICANT 303", 380, 190, AMBER, 8.0, glitch=4.0 if local < 0.35 else 0.0)

    d = ImageDraw.Draw(frame)
    d.line([(660, 660), (1260, 660)], fill=AMBER + (200,), width=2)
    spaced_text(d, (W / 2, 700), "HARD TECHNO // ACID PROTOCOL",
                font(FONT_MONO, 30), CYAN + (255,), 3.0, anchor="ma")
    d.text((W / 2, 780), "VIRTUAL ANALOG BASS LINE SYNTHESIZER",
           font=font(FONT_MONO_REG, 22), fill=MUTED + (230,), anchor="ma")
    d.text((W / 2, 830), "VST3   ·   AU   ·   STANDALONE",
           font=font(FONT_MONO_REG, 20), fill=MUTED + (170,), anchor="ma")


def scene_panel(frame, t, local):
    draw_ui(frame, PANEL_GREEN, (150, 176, 1620, 886), AMBER, scan_t=local * 0.55)
    caption(frame, 150, 26, "01 / THE INSTRUMENT", "PROCEDURAL 3D PANEL",
            "PCB  ·  ACRYLIC  ·  SPUN ALUMINIUM  ·  7-SEGMENT CLOCK")


def scene_drawer(frame, t, local):
    rect = (966, 130, 894, 820)
    draw_ui(frame, DRAWER_GREEN, rect, CYAN, scan_t=local * 0.8)

    scale = rect[3] / 990.0
    step = int((t / (60.0 / 138.0 / 4))) % 16
    x = rect[0] + (64 + step * 49.75) * scale
    y = rect[1] + 666 * scale
    h = 200 * scale
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    ld.rectangle([x, y, x + 49.75 * scale, y + h], fill=CYAN + (55,))
    ld.polygon([(x + 14, y - 13), (x + 27, y - 13), (x + 20, y - 3)], fill=CYAN + (230,))
    frame.alpha_composite(layer)

    # Play LED.
    d = ImageDraw.Draw(frame)
    px = rect[0] + 56 * scale
    py = rect[1] + 644 * scale
    d.ellipse([px - 7, py - 7, px + 7, py + 7], fill=(255, 40, 50, 80))
    d.ellipse([px - 3, py - 3, px + 3, py + 3], fill=(255, 60, 70, 255))

    d.line([(90, 250), (170, 250)], fill=CYAN + (255,), width=3)
    d.text((90, 266), "02 / PATTERN CORE", font=font(FONT_MONO, 24), fill=CYAN + (255,))
    spaced_text(d, (90, 320), "SIXTEEN", font(FONT_TITLE, 84), WHITE + (255,), 2.0)
    spaced_text(d, (90, 402), "STEPS.", font(FONT_TITLE, 84), AMBER + (255,), 2.0)
    spaced_text(d, (90, 484), "NO MERCY.", font(FONT_TITLE, 84), WHITE + (255,), 2.0)
    for i, line in enumerate(("EIGHT PATTERNS. GATE, ACCENT,",
                              "SLIDE, SWING AND LENGTH —",
                              "SET PER STEP. EXPORT ANY",
                              "PATTERN AS A STANDARD MIDI FILE.")):
        d.text((90, 610 + i * 34), line, font=font(FONT_MONO_REG, 21), fill=MUTED + (235,))


def scene_drawer_close(frame, t, local):
    # Crop the transport + grid + lanes + actions and scale them up.
    crop = DRAWER_GREEN.crop((0, 612, 1080, 984))  # 1080 x 372
    rect = (160, 230, 1600, 551)
    shot = crop.resize((rect[2], rect[3]), Image.LANCZOS)
    draw_ui(frame, shot, rect, CYAN, scan_t=local * 1.2)

    sx = rect[2] / 1080.0
    sy = rect[3] / 372.0
    step = int((t / (60.0 / 138.0 / 4))) % 16
    x = rect[0] + (64 + step * 49.75) * sx
    y = rect[1] + (666 - 612) * sy
    h = 200 * sy
    layer = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    ld = ImageDraw.Draw(layer)
    ld.rectangle([x, y, x + 49.75 * (rect[2] / 900.0), y + h], fill=CYAN + (55,))
    frame.alpha_composite(layer)

    caption(frame, 150, 100, "03 / NOTE ENTRY", "PAINT YOUR OWN LINE",
            "CLICK A CELL TO SET THE NOTE  ·  RIGHT-CLICK TO MUTE  ·  DRAG TO PAINT")


def scene_red(frame, t, local):
    draw_ui(frame, PANEL_RED, (150, 176, 1620, 886), RED, scan_t=local * 0.5)
    caption(frame, 150, 26, "04 / OBSIDIAN RED", "BUILT FOR THE NIGHT",
            "BLACK ACRYLIC  ·  COPPER TRACES  ·  RED SILKSCREEN")


def scene_red_drawer(frame, t, local):
    draw_ui(frame, DRAWER_RED, (520, 150, 880, 806), RED, scan_t=local * 0.7)
    d = ImageDraw.Draw(frame)
    spaced_text(d, (W / 2, 26), "SQUELCH.  SLIDE.  SCREAM.",
                font(FONT_TITLE, 62), WHITE + (245,), 4.0, anchor="ma")
    d.text((W / 2, 104), "OBSIDIAN RED  //  NO LIGHTS. NO MERCY.",
           font=font(FONT_MONO_REG, 20), fill=MUTED + (220,), anchor="ma")


def scene_feature(frame, t, local, index, title, sub):
    d = ImageDraw.Draw(frame)
    d.text((150, 250), f"0{index} / 05", font=font(FONT_MONO, 26), fill=CYAN + (220,))
    d.line([(150, 310), (150 + 140, 310)], fill=AMBER + (255,), width=3)

    spaced_text(d, (150, 360), title, font(FONT_TITLE, 96), AMBER + (255,), 2.0)

    d.text((150, 500), sub, font=font(FONT_MONO, 30), fill=WHITE + (235,))

    # Decorative waveform.
    pts = []
    for i in range(0, 1600, 8):
        x = 150 + i
        y = 800 + math.sin(i * 0.02 + t * 3.0) * 34 * math.sin(i * 0.004 + t)
        pts.append((x, y))
    if len(pts) > 1:
        d.line(pts, fill=AMBER + (70,), width=2)


def scene_end(frame, t, local):
    d = ImageDraw.Draw(frame)

    # Emblem.
    cx, cy = W / 2, 300
    glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([cx - 62, cy - 62, cx + 62, cy + 62], outline=AMBER + (255,), width=3)
    wave = [(cx - 42 + i * 2, cy + 16 * math.sin(i * 0.20)) for i in range(43)]
    gd.line(wave, fill=AMBER + (255,), width=5)
    frame.alpha_composite(glow.filter(ImageFilter.GaussianBlur(12)))
    d.ellipse([cx - 62, cy - 62, cx + 62, cy + 62], outline=AMBER + (230,), width=3)
    d.line(wave, fill=AMBER + (255,), width=5)

    big_title(frame, "REPLICANT 303", 400, 150, AMBER, 7.0)

    d.line([(700, 610), (1220, 610)], fill=AMBER + (180,), width=2)
    spaced_text(d, (W / 2, 650), "DESIGNED FOR THE UNDERGROUND",
                font(FONT_MONO, 26), CYAN + (240,), 2.5, anchor="ma")
    d.text((W / 2, 720), "VST3   ·   AU   ·   STANDALONE",
           font=font(FONT_MONO_REG, 22), fill=MUTED + (220,), anchor="ma")
    d.text((W / 2, 790), "github.com/DanimanX/Replicant303",
           font=font(FONT_MONO_REG, 24), fill=WHITE + (230,), anchor="ma")
    d.text((W / 2, 840), "DANIMANX", font=font(FONT_MONO, 20), fill=AMBER + (180,), anchor="ma")


# ---------------------------------------------------------------------------
# Frame assembly
# ---------------------------------------------------------------------------
def load_png(name):
    return Image.open(os.path.join(ASSETS, name)).convert("RGB")


CITY = None
FOG = None
VIGNETTE = None
SCAN = None
NOISE = None
BG = None


def init_visuals():
    global CITY, FOG, VIGNETTE, SCAN, NOISE, BG
    global PANEL_GREEN, PANEL_RED, DRAWER_GREEN, DRAWER_RED
    CITY = make_city()
    FOG = make_fog()
    VIGNETTE = make_vignette()
    SCAN = make_scanlines()
    NOISE = make_noise_tiles()
    BG = gradient_background()
    PANEL_GREEN = load_png("replicant303_ui.png")
    PANEL_RED = load_png("replicant303_ui_obsidian_red.png")
    DRAWER_GREEN = load_png("replicant303_sequencer.png")
    DRAWER_RED = load_png("replicant303_sequencer_obsidian_red.png")


def render_frame(t):
    frame = BG.convert("RGBA").copy()
    frame.alpha_composite(CITY)
    frame.alpha_composite(FOG, (int(30 * math.sin(t * 0.13)), int(12 * math.sin(t * 0.21))))
    draw_rain(frame, t, 1.0 if t < 8 or t > 52 else 0.55)

    # Beat glow in the sky.
    pulse = kick_pulse(t)
    if pulse > 0.01:
        glow = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        gd = ImageDraw.Draw(glow)
        gd.ellipse([W / 2 - 520, 120, W / 2 + 520, 700], fill=(255, 140, 50, int(10 + 26 * pulse)))
        frame.alpha_composite(glow.filter(ImageFilter.GaussianBlur(80)))

    if t < 6.0:
        scene_intro(frame, t)
    elif t < 14.0:
        scene_title(frame, t, t - 6.0)
    elif t < 26.0:
        scene_panel(frame, t, t - 14.0)
    elif t < 33.0:
        scene_drawer(frame, t, t - 26.0)
    elif t < 40.0:
        scene_drawer_close(frame, t, t - 33.0)
    elif t < 46.0:
        scene_red(frame, t, t - 40.0)
    elif t < 51.0:
        scene_red_drawer(frame, t, t - 46.0)
    elif t < 53.0:
        scene_feature(frame, t, t - 51.0, 1, "5 DISTORTION MODELS",
                      "POCO RAT  ·  TUBE  ·  HARD CLIP  ·  GERMAN FUZZ  ·  FOLDER")
    elif t < 55.0:
        scene_feature(frame, t, t - 53.0, 2, "HOST SYNC OR FREE CLOCK",
                      "FOLLOW YOUR DAW TRANSPORT  ·  OR RUN STANDALONE")
    elif t < 57.0:
        scene_feature(frame, t, t - 55.0, 3, "EXPORT YOUR PATTERNS",
                      "16-STEP MIDI FILES  ·  ACCENTS AND SLIDES INCLUDED")
    else:
        scene_end(frame, t, t - 57.0)

    # Whole-frame finishing.
    frame.alpha_composite(SCAN)
    frame.alpha_composite(VIGNETTE)

    if t > 0.4:
        frame.alpha_composite(NOISE[int(t * 30) % len(NOISE)])

    # Fades.
    if t < 0.5:
        k = t / 0.5
        frame = Image.eval(frame, lambda v: int(v * k))
    if t > DURATION - 2.0:
        k = max(0.0, (DURATION - t) / 2.0)
        frame = Image.eval(frame, lambda v: int(v * k))

    # Glitch cuts.
    cuts = (6.0, 14.0, 26.0, 33.0, 40.0, 46.0, 51.0, 53.0, 55.0, 57.0)
    for cut in cuts:
        if 0 <= t - cut < 0.12:
            arr = np.array(frame)
            rng = np.random.default_rng(int(t * 10000) + int(cut))
            for _ in range(5):
                y0 = int(rng.integers(0, H - 60))
                hgt = int(rng.integers(10, 60))
                arr[y0:y0 + hgt] = np.roll(arr[y0:y0 + hgt], int(rng.integers(-40, 40)), axis=1)
            frame = Image.fromarray(arr)
            break

    return frame.convert("RGB")


def main():
    audio_only = "--audio-only" in sys.argv
    frames_only = "--frames-only" in sys.argv

    if not frames_only:
        mix_path = build_audio()
    else:
        mix_path = os.path.join(ASSETS, "replicant303_hardtechno_mix.wav")

    if audio_only:
        return

    init_visuals()

    out_path = os.path.join(ASSETS, "replicant303_promo_hardtechno_1080p.mp4")
    poster_path = os.path.join(ASSETS, "replicant303_promo_hardtechno_poster.png")

    cmd = [
        "ffmpeg", "-y", "-v", "error",
        "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{W}x{H}", "-r", str(FPS), "-i", "-",
        "-i", mix_path,
        "-c:v", "libx264", "-preset", "medium", "-crf", "18",
        "-pix_fmt", "yuv420p", "-movflags", "+faststart",
        "-c:a", "aac", "-b:a", "192k",
        "-shortest", out_path,
    ]

    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    total = int(DURATION * FPS)

    for i in range(total):
        t = i / FPS
        frame = render_frame(t)
        if i == int(31.0 * FPS):
            frame.save(poster_path)
        proc.stdin.write(frame.tobytes())
        if i % 150 == 0:
            print(f"[video] frame {i}/{total}  ({t:5.1f}s)")

    proc.stdin.close()
    proc.wait()

    if proc.returncode != 0:
        print("[error] ffmpeg failed")
        sys.exit(1)

    print(f"[video] wrote {out_path}")
    print(f"[video] poster {poster_path}")


if __name__ == "__main__":
    main()
