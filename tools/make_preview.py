#!/usr/bin/env python3
"""
Replicant303 - README image builder.

Regenerates the two composite images the README uses, from the real
generate_snapshot output:

    assets/replicant303_ui_preview.png   both skins + the preset vault strip
    assets/replicant303_presets.png      8 banks x 64 presets diagram

Usage:
    python3 tools/make_preview.py

The individual panel / drawer screenshots are produced first:

    ./build/generate_snapshot assets/replicant303_ui.png 0 0
    ./build/generate_snapshot assets/replicant303_ui_obsidian_red.png 1 0
    ./build/generate_snapshot assets/replicant303_sequencer.png 0 1 200
    ./build/generate_snapshot assets/replicant303_sequencer_obsidian_red.png 1 1 400
"""

import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets")

AMBER = (255, 184, 92)
AMBER_HOT = (255, 224, 170)
CYAN = (92, 226, 255)
RED = (255, 59, 66)
GREEN = (30, 200, 54)
WHITE = (232, 240, 238)
MUTED = (150, 168, 168)
DARK = (5, 7, 10)

FONT_TITLE = "/usr/share/fonts/TTF/RobotoCondensed-Bold.ttf"
FONT_MONO = "/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Bold.ttf"
FONT_MONO_REG = "/usr/share/fonts/TTF/JetBrainsMonoNerdFontMono-Regular.ttf"

# Preset row geometry inside the drawer screenshot (panel 590 + drawer row).
PRESET_STRIP = (0, 944, 1080, 1030)
PANEL_H = 590

_cache = {}


def font(path, size):
    key = (path, size)
    if key not in _cache:
        _cache[key] = ImageFont.truetype(path, size)
    return _cache[key]


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


def load(name):
    return Image.open(os.path.join(ASSETS, name)).convert("RGBA")


def hardware_background(w, h, top=(10, 16, 22), bottom=(2, 3, 5)):
    img = Image.new("RGB", (w, h), bottom)
    px = img.load()
    for y in range(h):
        k = y / max(1, h - 1)
        c = tuple(int(top[i] * (1 - k) + bottom[i] * k) for i in range(3))
        for x in range(w):
            px[x, y] = c
    return img.convert("RGBA")


def add_traces(img, colour, alpha=34):
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    w, h = img.size
    for i in range(10):
        y = 60 + i * (h // 10)
        d.line([(0, y), (w * 0.22, y)], fill=colour + (alpha,), width=1)
        d.line([(w * 0.22, y), (w * 0.27, y + 14)], fill=colour + (alpha,), width=1)
        d.line([(w * 0.27, y + 14), (w, y + 14)], fill=colour + (alpha,), width=1)
    img.alpha_composite(layer)


def add_scanlines(img, alpha=18):
    w, h = img.size
    layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    for y in range(0, h, 4):
        d.line([(0, y), (w, y)], fill=(0, 0, 0, alpha))
    img.alpha_composite(layer)


def panel_block(shot, label, sub, accent):
    """Returns a 1080-wide card: header bar + panel + preset vault strip."""
    w = shot.width
    header_h = 44
    strip = shot.crop((0, PRESET_STRIP[1], shot.width, PRESET_STRIP[3]))
    strip_h = strip.height + 12

    card = Image.new("RGBA", (w, header_h + PANEL_H + strip_h), DARK + (255,))
    card.alpha_composite(shot.crop((0, 0, shot.width, PANEL_H)), (0, header_h))

    d = ImageDraw.Draw(card)
    d.rectangle([0, 0, w, header_h - 1], fill=(9, 13, 17, 255))
    d.line([(0, header_h - 1), (w, header_h - 1)], fill=accent + (200,), width=2)
    d.rectangle([18, 14, 26, 30], fill=accent + (255,))
    d.text((38, 10), label, font=font(FONT_MONO, 22), fill=WHITE + (255,))
    d.text((w - 18, 12), sub, font=font(FONT_MONO_REG, 17), fill=MUTED + (220,), anchor="ra")

    # Preset strip on a recessed bar, framed in the same accent.
    y0 = header_h + PANEL_H
    d.rectangle([0, y0, w, y0 + strip_h], fill=(7, 10, 13, 255))
    d.line([(0, y0), (w, y0)], fill=accent + (110,), width=1)
    card.alpha_composite(strip, (0, y0 + 6))
    d.rectangle([0, y0 + 1, w - 1, y0 + strip_h - 1], outline=accent + (70,), width=1)
    return card


def build_ui_preview():
    green = load("replicant303_ui.png")
    red = load("replicant303_ui_obsidian_red.png")
    green_drawer = load("replicant303_sequencer.png")
    red_drawer = load("replicant303_sequencer_obsidian_red.png")

    # The panel screenshots are skin-only; the drawer screenshots carry the
    # vault row. Combine each skin's panel with its own preset strip.
    cards = [
        panel_block(green, "TD-3 TRANSLUCENT GREEN",
                    "PATTERN CORE  ·  PRESET VAULT 8 x 64", GREEN),
        panel_block(red, "OBSIDIAN RED",
                    "BLACK ACRYLIC  ·  RED SILKSCREEN  ·  PRESET VAULT 8 x 64", RED),
    ]

    # Swap in the drawer's preset strip (the panel-only shot has no vault row).
    for card, drawer, accent in ((cards[0], green_drawer, GREEN),
                                 (cards[1], red_drawer, RED)):
        y0 = 44 + PANEL_H
        strip = drawer.crop((0, PRESET_STRIP[1], drawer.width, PRESET_STRIP[3]))
        d = ImageDraw.Draw(card)
        d.rectangle([0, y0, card.width, card.height], fill=(7, 10, 13, 255))
        d.line([(0, y0), (card.width, y0)], fill=accent + (110,), width=1)
        card.alpha_composite(strip, (0, y0 + 6))
        d.rectangle([0, y0 + 1, card.width - 1, card.height - 1],
                    outline=accent + (70,), width=1)

    margin = 20
    gap = 18
    width = cards[0].width + margin * 2
    height = margin + sum(c.height for c in cards) + gap + margin

    out = hardware_background(width, height, (8, 12, 17), (2, 3, 5))
    add_traces(out, AMBER, 20)
    y = margin
    for card in cards:
        out.alpha_composite(card, (margin, y))
        y += card.height + gap

    add_scanlines(out, 14)
    out.convert("RGB").save(os.path.join(ASSETS, "replicant303_ui_preview.png"))
    print(f"[preview] wrote {width}x{height} ui preview")


def build_preset_diagram():
    w, h = 1600, 660
    img = hardware_background(w, h, (11, 17, 22), (2, 3, 5))
    add_traces(img, CYAN, 26)

    edge = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ed = ImageDraw.Draw(edge)
    ed.rectangle([10, 10, w - 11, h - 11], outline=AMBER + (120,), width=1)
    img.alpha_composite(edge)

    d = ImageDraw.Draw(img)
    d.text((46, 36), "PRESET VAULT", font=font(FONT_TITLE, 58), fill=AMBER + (255,))
    d.text((48, 104), "EIGHT BANKS  ·  ONE PER PATTERN LETTER  ·  64 COMPLETE PATCHES EACH",
           font=font(FONT_MONO_REG, 20), fill=MUTED + (235,))

    total = spaced_width(d, "512", font(FONT_TITLE, 92), 2.0)
    x = w - 48 - total
    spaced_text(d, (x, 40), "512", font(FONT_TITLE, 92), AMBER_HOT + (255,), 2.0)
    d.text((w - 48, 138), "PRESETS", font=font(FONT_MONO, 18), fill=CYAN + (230,), anchor="ra")

    letters = "ABCDEFGH"
    grid_x = 150
    grid_y = 178
    cell_w, cell_h = 154, 40
    gap_x, gap_y = 10, 7

    for bank in range(8):
        y = grid_y + bank * (cell_h + gap_y)
        accent = GREEN if bank % 2 == 0 else CYAN

        d.text((60, y + 8), letters[bank], font=font(FONT_TITLE, 30), fill=accent + (255,))
        d.text((88, y + 16), "64", font=font(FONT_MONO, 13), fill=MUTED + (200,))

        for page in range(8):
            x = grid_x + page * (cell_w + gap_x)
            active = (bank == 3 and page == 1)
            face = (38, 30, 18, 255) if active else (13, 19, 24, 255)
            d.rounded_rectangle([x, y, x + cell_w, y + cell_h], radius=4,
                                fill=face, outline=(AMBER if active else accent) + (255 if active else 60,),
                                width=2 if active else 1)

            d.text((x + 10, y + 6), f"P{page + 1}" + ("  LOADED" if active else ""),
                   font=font(FONT_MONO, 12),
                   fill=(AMBER_HOT if active else MUTED) + (255,))

            # Eight slot ticks per page.
            for slot in range(8):
                tx = x + 48 + slot * 12
                lit = active and slot in (0, 1)
                d.rectangle([tx, y + 18, tx + 7, y + 24],
                            fill=(AMBER if lit else accent) + (235 if lit else 70,))


    # Action row.
    chips = [
        ("GEN", "PROCEDURAL VARIATION", AMBER),
        ("STORE", "CAPTURE THE LIVE PATCH", GREEN),
        ("IMPORT", ".MIDI -> SLOT", CYAN),
        ("EXPORT", "SLOT -> .MIDI", RED),
    ]

    y = 566
    chip_w = (w - 96 - 3 * 14) // 4
    for i, (name, text, accent) in enumerate(chips):
        x = 48 + i * (chip_w + 14)
        d.rounded_rectangle([x, y, x + chip_w, y + 62], radius=5,
                            fill=(12, 18, 23, 255), outline=accent + (190,), width=2)
        d.rectangle([x + 14, y + 18, x + 20, y + 44], fill=accent + (255,))
        d.text((x + 34, y + 11), name, font=font(FONT_MONO, 21), fill=accent + (255,))
        d.text((x + 34, y + 37), text, font=font(FONT_MONO_REG, 14), fill=MUTED + (225,))

    add_scanlines(img, 16)
    img.convert("RGB").save(os.path.join(ASSETS, "replicant303_presets.png"))
    print(f"[preview] wrote {w}x{h} preset diagram")


def main():
    build_ui_preview()
    build_preset_diagram()


if __name__ == "__main__":
    main()
