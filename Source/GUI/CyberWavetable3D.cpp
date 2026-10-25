#include "CyberWavetable3D.h"

#include <algorithm>
#include <cmath>

namespace Replicant303
{
namespace
{
    constexpr float twoPi = 6.283185307f;

    const juce::Colour kCyan    { 0xff00e5ff };
    const juce::Colour kViolet  { 0xff7b5cff };
    const juce::Colour kMagenta { 0xffff2bd6 };
    const juce::Colour kChamber { 0xff050818 };

    std::atomic<bool>& forcedSoftwareFlag()
    {
        static std::atomic<bool> forced { false };
        return forced;
    }

    float wrap01 (float t) noexcept
    {
        return t - std::floor (t);
    }

    /** Colour ramp of the waterfall: magenta at the horizon, cyan up front. */
    juce::Colour depthColour (float depth) noexcept
    {
        const float d = juce::jlimit (0.0f, 1.0f, depth);

        if (d < 0.55f)
            return kMagenta.interpolatedWith (kViolet, d / 0.55f);

        return kViolet.interpolatedWith (kCyan, (d - 0.55f) / 0.45f);
    }

    //==========================================================================
    /** The idle hologram's signature bank.

        These six shapes are pure display art for the attract mode (shown only
        while no note is sounding). They deliberately never reach the audio
        path: the VCO keeps playing the authentic 303 saw/square.
    */
    const char* idleSignatureName (int index) noexcept
    {
        static const char* names[] =
        {
            "DATA BLADE", "NEON PULSE", "CHROME SINE",
            "GLITCH RUNNER", "LASER RAMP", "VOLT SPIKE"
        };

        return names[(size_t) juce::jlimit (0, 5, index)];
    }

    float idleSignature (int index, float t, double nowSeconds) noexcept
    {
        const float time = (float) nowSeconds;
        const float p = wrap01 (t);

        switch (juce::jlimit (0, 5, index))
        {
            case 0: // DATA BLADE: ramp with a cut notch
            {
                float v = 2.0f * p - 1.0f;
                const float notch = std::abs (p - 0.62f);

                if (notch < 0.05f)
                    v -= 1.2f * (1.0f - notch / 0.05f);

                return v;
            }

            case 1: // NEON PULSE: narrow pulse with an overshoot
            {
                const bool high = wrap01 (p + 0.10f) < 0.22f;
                float v = high ? 1.0f : -1.0f;

                if (high && p < 0.06f)
                    v += 0.35f * (1.0f - p / 0.06f);

                return v;
            }

            case 2: // CHROME SINE: stepped sine
                return std::round (std::sin (twoPi * p) * 4.0f) / 4.0f;

            case 3: // GLITCH RUNNER: saw broken into data blocks
            {
                float v = 2.0f * wrap01 (p * 2.0f) - 1.0f;
                const int block = (int) (p * 16.0f);
                v += 0.22f * std::sin ((float) block * 2.399f + time * 3.0f);
                return v;
            }

            case 4: // LASER RAMP: exponential rise, hard snap back
            {
                const float expo = (std::exp (3.2f * p) - 1.0f) / (std::exp (3.2f) - 1.0f);
                return 2.0f * expo - 1.0f;
            }

            default: // VOLT SPIKE: square with decaying ringing
            {
                const float base = p < 0.5f ? 1.0f : -1.0f;
                const float ring = 0.55f * std::exp (-7.0f * p) * std::sin (twoPi * 9.0f * p);
                return base * 0.75f + ring;
            }
        }
    }
}

//==============================================================================
CyberWavetable3D::CyberWavetable3D()
{
    setOpaque (false);
    setInterceptsMouseClicks (true, false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

CyberWavetable3D::~CyberWavetable3D()
{
    stopTimer();
    detachGL();
}

void CyberWavetable3D::setSoftwareRenderingForced (bool shouldForce)
{
    forcedSoftwareFlag().store (shouldForce);
}

bool CyberWavetable3D::isSoftwareRenderingForced() noexcept
{
    return forcedSoftwareFlag().load();
}

void CyberWavetable3D::setExpanded (bool shouldBeExpanded) noexcept
{
    if (expanded == shouldBeExpanded)
        return;

    expanded = shouldBeExpanded;
    repaint();

    if (gpuActive.load())
        glContext.triggerRepaint();
}

//==============================================================================
void CyberWavetable3D::paint (juce::Graphics& g)
{
    const auto area = getLocalBounds().toFloat();
    if (area.getWidth() < 2.0f || area.getHeight() < 2.0f)
        return;

    const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    softwareScene = buildScene (area, now);

    // With a GL context the 3D scene is already on screen underneath this
    // component's texture, so paint() only contributes the chrome on top.
    if (! gpuActive.load())
        drawSceneSoftware (g, softwareScene);

    drawChrome (g, softwareScene);
}

void CyberWavetable3D::resized()
{
    cachedWidth.store (getWidth());
    cachedHeight.store (getHeight());

    if (gpuActive.load())
        glContext.triggerRepaint();
}

void CyberWavetable3D::parentHierarchyChanged()
{
    if (isShowing())
        attachGL();
}

void CyberWavetable3D::visibilityChanged()
{
    if (isShowing())
    {
        startTimerHz (60);
        attachGL();
    }
    else
    {
        stopTimer();
        detachGL();
    }
}

void CyberWavetable3D::mouseDown (const juce::MouseEvent&)
{
    if (onToggleRequested != nullptr)
        onToggleRequested();
}

void CyberWavetable3D::mouseEnter (const juce::MouseEvent&)
{
    hovering = true;
    repaint();
}

void CyberWavetable3D::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
    repaint();
}

//==============================================================================
void CyberWavetable3D::timerCallback()
{
    if (! glContext.isAttached() && ! glFailed
        && ! forcedSoftwareFlag().load() && isShowing())
    {
        attachGL();
    }

    if (! gpuActive.load())
    {
        // No GL: the software renderer needs a repaint per frame.
        if (glContext.isAttached())
        {
            if (++framesSinceAttach > 45)   // ~0.75 s without a live context
                fallBackToSoftware();
        }

        repaint();
        return;
    }

    glContext.triggerRepaint();

    // The scene animates on the GPU; only the HUD text needs a component
    // repaint, and that can stay at 6 Hz instead of uploading a texture at 60.
    if (++framesSinceHud >= 10)
    {
        framesSinceHud = 0;
        repaint();
    }
}

//==============================================================================
void CyberWavetable3D::attachGL()
{
    if (glFailed || forcedSoftwareFlag().load()
        || glContext.isAttached() || ! isShowing())
        return;

    glContext.setRenderer (this);
    glContext.setComponentPaintingEnabled (true);
    glContext.attachTo (*this);

    framesSinceAttach = 0;

    // JUCE only reports the context as attached once the native context exists.
    // Without one (headless, no driver) the software renderer takes over.
    if (! glContext.isAttached())
    {
        glContext.setRenderer (nullptr);
        glFailed = true;
        repaint();
    }
}

void CyberWavetable3D::detachGL()
{
    if (! glContext.isAttached())
        return;

    gpuActive.store (false);
    glContext.setRenderer (nullptr);
    glContext.detach();
}

void CyberWavetable3D::fallBackToSoftware()
{
    glFailed = true;
    detachGL();
    repaint();
}

void CyberWavetable3D::newOpenGLContextCreated()
{
    using namespace juce::gl;

    // Legacy immediate mode is what JUCE's default (non-core) context gives us
    // on macOS, Windows and Linux. If a driver hands us a core profile the
    // entry points are null, and the component must not pretend to render.
    const bool immediateModeAvailable = glBegin != nullptr && glEnd != nullptr
                                     && glVertex2f != nullptr && glColor4f != nullptr
                                     && glLineWidth != nullptr && glClear != nullptr;

    if (! immediateModeAvailable)
    {
        glFailed = true;

        juce::Component::SafePointer<CyberWavetable3D> safeThis (this);
        juce::MessageManager::callAsync ([safeThis]
        {
            if (safeThis != nullptr)
                safeThis->fallBackToSoftware();
        });
        return;
    }

    gpuActive.store (true);
    framesSinceHud = 0;

    // Drop the software-drawn frame that is still cached as this component's
    // texture, so only the chrome is composited over the GL scene.
    juce::Component::SafePointer<CyberWavetable3D> safeThis (this);
    juce::MessageManager::callAsync ([safeThis]
    {
        if (safeThis != nullptr)
            safeThis->repaint();
    });
}

void CyberWavetable3D::openGLContextClosing()
{
    gpuActive.store (false);
}

void CyberWavetable3D::renderOpenGL()
{
    if (! gpuActive.load())
        return;

    const int w = cachedWidth.load();
    const int h = cachedHeight.load();

    if (w < 4 || h < 4)
        return;

    const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    glScene = buildScene (juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h), now);

    drawSceneGL (glScene);
}

//==============================================================================
CyberWavetable3D::Scene CyberWavetable3D::buildScene (juce::Rectangle<float> area,
                                                      double nowSeconds) const
{
    Scene scene;

    const bool compact = area.getHeight() < 90.0f;

    auto plot = area.reduced (3.0f);
    plot.removeFromTop (compact ? 5.0f : 16.0f);
    plot.removeFromBottom (compact ? 3.0f : 14.0f);

    if (plot.getWidth() < 16.0f || plot.getHeight() < 12.0f)
        plot = area.reduced (2.0f);

    scene.plot = plot;
    scene.horizon = plot.getY() + plot.getHeight() * 0.10f;

    const int numRows = juce::jlimit (6, 12, (int) (plot.getHeight() / 30.0f));
    const int numPoints = juce::jlimit (24, 96, (int) (plot.getWidth() / 2.5f));

    const ScopeTap* tap = scopeTap;
    const bool live = tap != nullptr && tap->isLive();
    const double sampleRate = tap != nullptr ? tap->getSampleRate() : 48000.0;
    const float frequency = tap != nullptr ? juce::jmax (20.0f, tap->getFrequencyHz()) : 110.0f;

    scene.live = live;
    scene.frequency = frequency;
    scene.signatureIndex = (int) (std::fmod (nowSeconds / 7.0, 6.0));
    scene.waveName = (tap != nullptr && tap->getWaveformIndex() == 1) ? "SQUARE" : "SAWTOOTH";
    scene.signatureName = idleSignatureName (scene.signatureIndex);

    // One slice is one full oscillator cycle, resampled with linear
    // interpolation so the shape stays clean at any frequency.
    const double cycleSamples = juce::jlimit (24.0, 4096.0, sampleRate / (double) frequency);
    const int window = juce::jmin ((int) std::lround (cycleSamples) + 2, ScopeTap::capacity / 2);

    // Each slice advances an eighth of a cycle, so the reset edge shears across
    // the stack and the surface reads as a helix instead of a flat fan.
    const double hopBudget = (double) (ScopeTap::capacity - window) / (double) juce::jmax (1, numRows - 1);
    const int hop = juce::jmax (2, (int) std::lround (juce::jmin (hopBudget, cycleSamples / 8.0)));
    const int needed = juce::jmin (ScopeTap::capacity, window + (numRows - 1) * hop);

    std::vector<float> audio ((size_t) juce::jmax (1, needed), 0.0f);

    if (live)
        tap->copyLatest (audio.data(), (int) audio.size());

    const float glitchPhase = std::fmod ((float) nowSeconds, 4.1f);
    const float glitch = glitchPhase < 0.10f ? (1.0f - glitchPhase / 0.10f) : 0.0f;

    scene.rows.reserve ((size_t) numRows);

    for (int r = 0; r < numRows; ++r)
    {
        // depth: 0 at the horizon, 1 at the nearest slice.
        const float depth = (r + 1.0f) / (float) numRows;
        const float elevation = std::pow (1.0f - depth, 1.15f);

        const float nearBaseline = plot.getBottom() - plot.getHeight() * 0.125f;
        const float farBaseline  = plot.getY() + plot.getHeight() * 0.11f;

        SceneRow row;
        row.depth = depth;
        row.baseline = nearBaseline - elevation * (nearBaseline - farBaseline);
        row.points.resize ((size_t) numPoints);

        const float halfWidth = plot.getWidth() * (0.22f + 0.40f * depth);
        const float amplitude = plot.getHeight() * 0.100f * (0.35f + 0.65f * depth);

        // Yaw: the stack is viewed slightly off-axis, so the perspective plane
        // reads as 3D instead of a flat stack of lines.
        const float yaw = (depth - 0.5f) * plot.getWidth() * 0.18f;
        const float tear = glitch * 8.0f
                         * std::sin ((float) r * 2.3f + (float) nowSeconds * 33.0f);

        double sumSquares = 0.0;

        for (int i = 0; i < numPoints; ++i)
        {
            const float t = (numPoints > 1) ? (float) i / (float) (numPoints - 1) : 0.0f;
            float value = 0.0f;

            if (live)
            {
                const int rowStart = juce::jmax (0, needed - window - (numRows - 1 - r) * hop);
                const double pos = (double) rowStart + (double) t * cycleSamples;
                const int i0 = juce::jlimit (0, needed - 1, (int) pos);
                const int i1 = juce::jmin (needed - 1, i0 + 1);
                const float frac = (float) (pos - (double) i0);

                value = audio[(size_t) i0] + frac * (audio[(size_t) i1] - audio[(size_t) i0]);
            }
            else
            {
                value = idleSignature (scene.signatureIndex, t, nowSeconds + (double) r * 0.03);
            }

            sumSquares += (double) value * value;

            row.points[(size_t) i] = { plot.getCentreX() + (t - 0.5f) * 2.0f * halfWidth + yaw + tear,
                                       row.baseline - value * amplitude };
        }

        row.energy = (float) std::sqrt (sumSquares / (double) numPoints);
        scene.rows.push_back (std::move (row));
    }

    // Neon floor grid, converging on the horizon.
    for (int i = 0; i <= 8; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / 8.0f;
        scene.gridRays.emplace_back (juce::Point<float> (plot.getCentreX(), scene.horizon),
                                     juce::Point<float> (x, plot.getBottom()));
    }

    for (int i = 1; i <= 7; ++i)
    {
        const float k = (float) i / 7.0f;
        const float y = scene.horizon + (plot.getBottom() - scene.horizon) * std::pow (k, 1.8f);
        const float half = plot.getWidth() * 0.5f * (0.30f + 0.70f * k);
        scene.gridRungs.emplace_back (juce::Point<float> (plot.getCentreX() - half, y),
                                      juce::Point<float> (plot.getCentreX() + half, y));
    }

    scene.scanY = scene.horizon + (plot.getBottom() - scene.horizon)
                                 * (float) std::fmod (nowSeconds * 0.45, 1.0);

    return scene;
}

//==============================================================================
void CyberWavetable3D::drawSceneSoftware (juce::Graphics& g, const Scene& scene) const
{
    const auto area = getLocalBounds().toFloat();
    const auto& plot = scene.plot;

    if (scene.rows.empty() || plot.getWidth() < 4.0f)
        return;

    g.saveState();
    g.reduceClipRegion (getLocalBounds());

    // Recessed chamber with a horizon bloom.
    juce::ColourGradient chamber (kChamber.brighter (0.06f), area.getCentreX(), area.getY(),
                                  juce::Colour (0xff010208), area.getCentreX(), area.getBottom(),
                                  false);
    g.setGradientFill (chamber);
    g.fillRect (area);

    g.setColour (kMagenta.withAlpha (0.16f));
    g.fillRect (area.withY (scene.horizon - 1.0f).withHeight (2.0f));
    g.setColour (kMagenta.withAlpha (0.05f));
    g.fillRect (area.withY (scene.horizon - 6.0f).withHeight (12.0f));

    // Grid.
    g.setColour (kCyan.withAlpha (0.14f));
    for (const auto& ray : scene.gridRays)
        g.drawLine (juce::Line<float> (ray.first, ray.second), 0.7f);

    g.setColour (kCyan.withAlpha (0.20f));
    for (const auto& rung : scene.gridRungs)
        g.drawLine (juce::Line<float> (rung.first, rung.second), 0.7f);

    // Translucent sheet between consecutive slices.
    const size_t numRows = scene.rows.size();

    for (size_t r = 1; r < numRows; ++r)
    {
        const auto& nearRow = scene.rows[r];
        const auto& farRow = scene.rows[r - 1];

        if (nearRow.points.size() != farRow.points.size())
            continue;

        juce::Path sheet;
        sheet.startNewSubPath (farRow.points.front());

        for (const auto& p : farRow.points)
            sheet.lineTo (p);

        for (auto it = nearRow.points.rbegin(); it != nearRow.points.rend(); ++it)
            sheet.lineTo (*it);

        sheet.closeSubPath();

        const float alpha = 0.05f + 0.10f * nearRow.depth
                          + 0.07f * juce::jlimit (0.0f, 1.0f, nearRow.energy);
        g.setColour (depthColour (nearRow.depth).withAlpha (alpha));
        g.fillPath (sheet);
    }

    // Slices: wide dim pass, tight glow, then the crisp trace.
    for (size_t r = 0; r < numRows; ++r)
    {
        const auto& row = scene.rows[r];
        const auto colour = depthColour (row.depth);
        const float alpha = juce::jlimit (0.16f, 1.0f, 0.18f + 0.82f * row.depth);

        juce::Path trace;
        trace.startNewSubPath (row.points.front());

        for (const auto& p : row.points)
            trace.lineTo (p);

        g.setColour (colour.withAlpha (alpha * 0.05f));
        g.strokePath (trace, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour (colour.withAlpha (alpha * 0.13f));
        g.strokePath (trace, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour (colour.withAlpha (alpha * 0.30f));
        g.strokePath (trace, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        g.setColour (colour.withAlpha (alpha));
        g.strokePath (trace, juce::PathStrokeType (0.9f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // The nearest slice is the one the instrument is playing right now: give it
    // a hot filament so the eye lands there first.
    {
        juce::Path frontTrace;
        frontTrace.startNewSubPath (scene.rows.back().points.front());

        for (const auto& p : scene.rows.back().points)
            frontTrace.lineTo (p);

        g.setColour (juce::Colour (0xffb8fdff).withAlpha (0.55f));
        g.strokePath (frontTrace, juce::PathStrokeType (0.9f, juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
    }

    // Chromatic ghosts on the near slices (the cyberpunk tell).
    for (size_t r = numRows > 5 ? numRows - 5 : 0; r < numRows; ++r)
    {
        const auto& row = scene.rows[r];

        for (int side = -1; side <= 1; side += 2)
        {
            juce::Path ghost;
            ghost.startNewSubPath (row.points.front().translated ((float) side * 1.1f, 0.0f));

            for (const auto& p : row.points)
                ghost.lineTo (p.translated ((float) side * 1.1f, 0.0f));

            g.setColour ((side < 0 ? kMagenta : kCyan).withAlpha (0.22f * row.depth));
            g.strokePath (ghost, juce::PathStrokeType (0.8f));
        }
    }

    // Floor reflection of the nearest slice.
    const auto& front = scene.rows.back();
    juce::Path reflection;
    bool first = true;

    for (const auto& p : front.points)
    {
        const juce::Point<float> mirrored (p.x, 2.0f * front.baseline - p.y);

        if (first)
        {
            reflection.startNewSubPath (mirrored);
            first = false;
        }
        else
        {
            reflection.lineTo (mirrored);
        }
    }

    g.setColour (kCyan.withAlpha (0.16f));
    g.strokePath (reflection, juce::PathStrokeType (0.8f));

    // Scanner.
    g.setColour (kCyan.withAlpha (0.30f));
    g.drawLine (plot.getX(), scene.scanY, plot.getRight(), scene.scanY, 1.0f);

    // CRT scanlines + vignette.
    g.setColour (juce::Colours::black.withAlpha (0.16f));
    for (float y = area.getY(); y < area.getBottom(); y += 3.0f)
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());

    g.restoreState();
}

//==============================================================================
void CyberWavetable3D::drawSceneGL (const Scene& scene)
{
    using namespace juce::gl;

    const float w = (float) cachedWidth.load();
    const float h = (float) cachedHeight.load();

    if (w < 4.0f || h < 4.0f || scene.rows.empty())
        return;

    const float renderScale = (float) juce::jmax (1.0, glContext.getRenderingScale());

    auto nx = [w] (float x) { return 2.0f * x / w - 1.0f; };
    auto ny = [h] (float y) { return 1.0f - 2.0f * y / h; };

    glDisable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor ((GLfloat) (kChamber.getFloatRed() * 0.6f),
                  (GLfloat) (kChamber.getFloatGreen() * 0.6f),
                  (GLfloat) (kChamber.getFloatBlue() * 0.6f),
                  1.0f);
    glClear (GL_COLOR_BUFFER_BIT);

    const auto& plot = scene.plot;

    // Horizon bloom: additive horizontal bands.
    glBlendFunc (GL_SRC_ALPHA, GL_ONE);

    for (int i = 0; i < 5; ++i)
    {
        const float k = (float) i / 4.0f;
        const float y = scene.horizon + k * 22.0f;

        glLineWidth ((6.0f - 4.0f * k) * renderScale);
        glBegin (GL_LINES);
        glColor4f (kMagenta.getFloatRed(), kMagenta.getFloatGreen(), kMagenta.getFloatBlue(),
                   0.05f * (1.0f - k));
        glVertex2f (nx (plot.getX()), ny (y));
        glVertex2f (nx (plot.getRight()), ny (y));
        glEnd();
    }

    // Grid.
    glLineWidth (1.0f * renderScale);
    glBegin (GL_LINES);

    for (const auto& ray : scene.gridRays)
    {
        glColor4f (kCyan.getFloatRed(), kCyan.getFloatGreen(), kCyan.getFloatBlue(), 0.10f);
        glVertex2f (nx (ray.first.x), ny (ray.first.y));
        glVertex2f (nx (ray.second.x), ny (ray.second.y));
    }

    for (const auto& rung : scene.gridRungs)
    {
        glColor4f (kCyan.getFloatRed(), kCyan.getFloatGreen(), kCyan.getFloatBlue(), 0.16f);
        glVertex2f (nx (rung.first.x), ny (rung.first.y));
        glVertex2f (nx (rung.second.x), ny (rung.second.y));
    }

    glEnd();

    // Translucent sheet: one triangle strip zig-zagging through the rows.
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin (GL_TRIANGLE_STRIP);

    for (size_t r = 0; r < scene.rows.size(); ++r)
    {
        const auto& row = scene.rows[r];
        const auto colour = depthColour (row.depth);
        const auto alpha = 0.06f + 0.11f * row.depth
                         + 0.07f * juce::jlimit (0.0f, 1.0f, row.energy);

        glColor4f (colour.getFloatRed(), colour.getFloatGreen(), colour.getFloatBlue(), alpha);

        const int n = (int) row.points.size();
        const bool forward = (r % 2 == 0);

        for (int k = 0; k < n; ++k)
        {
            const auto& p = row.points[(size_t) (forward ? k : n - 1 - k)];
            glVertex2f (nx (p.x), ny (p.y));
        }
    }

    glEnd();

    // Glow: offset copies with additive blending. Many drivers clamp
    // glLineWidth to 1, so the bloom is built from geometry, not from width.
    glBlendFunc (GL_SRC_ALPHA, GL_ONE);
    glLineWidth (1.0f * renderScale);

    for (const auto& row : scene.rows)
    {
        const auto colour = depthColour (row.depth);
        const auto alpha = juce::jlimit (0.16f, 1.0f, 0.18f + 0.82f * row.depth);

        for (int pass = 0; pass < 3; ++pass)
        {
            const float offset = (pass == 0 ? -2.4f : (pass == 1 ? 2.4f : 0.0f)) * renderScale;
            const float gain = (pass == 2 ? 0.20f : 0.11f);

            glColor4f (colour.getFloatRed(), colour.getFloatGreen(), colour.getFloatBlue(),
                       alpha * gain);
            glBegin (GL_LINE_STRIP);

            for (const auto& p : row.points)
                glVertex2f (nx (p.x), ny (p.y + offset));

            glEnd();
        }
    }

    // Chromatic ghosts on the near slices.
    glLineWidth (1.0f * renderScale);

    for (size_t r = scene.rows.size() > 5 ? scene.rows.size() - 5 : 0; r < scene.rows.size(); ++r)
    {
        const auto& row = scene.rows[r];

        for (int side = -1; side <= 1; side += 2)
        {
            const auto& tint = (side < 0) ? kMagenta : kCyan;

            glColor4f (tint.getFloatRed(), tint.getFloatGreen(), tint.getFloatBlue(),
                       0.20f * row.depth);
            glBegin (GL_LINE_STRIP);

            for (const auto& p : row.points)
                glVertex2f (nx (p.x + (float) side * 1.1f), ny (p.y));

            glEnd();
        }
    }

    // Crisp traces on top.
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const auto& row : scene.rows)
    {
        const auto colour = depthColour (row.depth);
        const auto alpha = juce::jlimit (0.16f, 1.0f, 0.18f + 0.82f * row.depth);

        glColor4f (colour.getFloatRed(), colour.getFloatGreen(), colour.getFloatBlue(), alpha);
        glBegin (GL_LINE_STRIP);

        for (const auto& p : row.points)
            glVertex2f (nx (p.x), ny (p.y));

        glEnd();
    }

    // Floor reflection and scanner.
    const auto& front = scene.rows.back();

    glBlendFunc (GL_SRC_ALPHA, GL_ONE);
    glColor4f (kCyan.getFloatRed(), kCyan.getFloatGreen(), kCyan.getFloatBlue(), 0.16f);
    glBegin (GL_LINE_STRIP);

    for (const auto& p : front.points)
        glVertex2f (nx (p.x), ny (2.0f * front.baseline - p.y));

    glEnd();

    glLineWidth (1.0f * renderScale);
    glColor4f (kCyan.getFloatRed(), kCyan.getFloatGreen(), kCyan.getFloatBlue(), 0.30f);
    glBegin (GL_LINES);
    glVertex2f (nx (plot.getX()), ny (scene.scanY));
    glVertex2f (nx (plot.getRight()), ny (scene.scanY));
    glEnd();

    // Hot filament on the slice that is playing right now.
    glColor4f (0.72f, 0.99f, 1.0f, 0.55f);
    glBegin (GL_LINE_STRIP);

    for (const auto& p : front.points)
        glVertex2f (nx (p.x), ny (p.y));

    glEnd();

    // CRT scanlines, composited normally over the bloom.
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f (0.0f, 0.0f, 0.0f, 0.16f);
    glBegin (GL_LINES);

    for (float y = 0.0f; y < h; y += 3.0f)
    {
        glVertex2f (nx (0.0f), ny (y));
        glVertex2f (nx (w), ny (y));
    }

    glEnd();

    glLineWidth (1.0f);
}

//==============================================================================
void CyberWavetable3D::drawChrome (juce::Graphics& g, const Scene& scene) const
{
    const auto area = getLocalBounds().toFloat();
    const bool compact = area.getHeight() < 90.0f;

    if (area.getWidth() < 20.0f)
        return;

    // Frame + corner ticks.
    g.setColour (kCyan.withAlpha (hovering ? 0.8f : 0.38f));
    g.drawRect (area.reduced (0.5f), 1.0f);

    const float tick = juce::jmin (11.0f, area.getHeight() * 0.24f);
    const auto inner = area.reduced (2.0f);

    g.setColour (kMagenta.withAlpha (0.9f));

    for (int corner = 0; corner < 4; ++corner)
    {
        const bool right = (corner & 1) != 0;
        const bool bottom = (corner & 2) != 0;
        const float x = right ? inner.getRight() : inner.getX();
        const float y = bottom ? inner.getBottom() : inner.getY();
        const float dx = right ? -tick : tick;
        const float dy = bottom ? -tick : tick;

        g.drawLine (x, y, x + dx, y, 1.4f);
        g.drawLine (x, y, x, y + dy, 1.4f);
    }

    const float fontSize = juce::jlimit (6.0f, 9.5f, area.getHeight() * 0.085f);
    const juce::Font font ((juce::FontOptions()).withHeight (fontSize).withStyle ("Bold"));
    const juce::Font smallFont ((juce::FontOptions()).withHeight (fontSize * 0.9f).withStyle ("Bold"));
    const juce::Font tinyFont ((juce::FontOptions()).withHeight (fontSize * 0.85f).withStyle ("Bold"));

    g.setFont (font);

    if (compact)
    {
        g.setColour (kCyan.withAlpha (0.85f));
        g.drawText ("3D", getLocalBounds().reduced (4, 2).removeFromTop (11),
                    juce::Justification::centredRight);
        return;
    }

    auto header = getLocalBounds().reduced (7, 4).removeFromTop (13);
    auto badgeArea = header.removeFromRight (46);

    g.setColour (kCyan.withAlpha (0.92f));
    g.drawText (expanded ? "WAVETABLE // 303 HOLOGRAM" : "WAVETABLE // 3D",
                header, juce::Justification::centredLeft);

    // GPU / SW badge.
    g.setColour ((gpuActive.load() ? kCyan : kViolet).withAlpha (0.18f));
    g.fillRoundedRectangle (badgeArea.toFloat(), 2.0f);
    g.setColour ((gpuActive.load() ? kCyan : kViolet).withAlpha (0.85f));
    g.drawRoundedRectangle (badgeArea.toFloat(), 2.0f, 0.8f);
    g.setFont (smallFont);
    g.drawText (gpuActive.load() ? "GPU" : "SW", badgeArea, juce::Justification::centred);

    // Footer: what is actually on screen.
    auto footer = getLocalBounds().reduced (7, 4).removeFromBottom (12);
    auto stateArea = footer.removeFromRight (42);

    g.setFont (font);
    g.setColour (kCyan.withAlpha (0.85f));

    if (scene.live)
    {
        g.drawText ("VCO  " + juce::String (scene.waveName) + "  "
                        + juce::String (scene.frequency, 1) + " Hz",
                    footer, juce::Justification::centredLeft);
        g.setColour (kMagenta.withAlpha (0.9f));
        g.drawText ("LIVE", stateArea, juce::Justification::centredRight);
    }
    else
    {
        g.drawText ("IDLE SIGNATURE  " + juce::String (scene.signatureName),
                    footer, juce::Justification::centredLeft);
        g.setColour (kViolet.withAlpha (0.9f));
        g.drawText ("IDLE", stateArea, juce::Justification::centredRight);
    }

    juce::ignoreUnused (tinyFont);
}
}
