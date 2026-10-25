#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>

#include <atomic>
#include <functional>
#include <utility>
#include <vector>

#include "DSP/ScopeTap.h"

namespace Replicant303
{
    //==========================================================================
    /** Cyberpunk 3D wavetable scope.

        The scene is a perspective waterfall: every slice is one cycle of the
        oscillator, older cycles recede towards the horizon and fade from cyan
        into magenta over a neon floor grid. OpenGL rasterises the surface, the
        ribbons and the glow passes; when no GL context can be created the
        component falls back to an equivalent software renderer, which is also
        what the offscreen snapshot tool uses.

        The scope is a pure visualiser: it reads the raw VCO through a lock-free
        tap and never touches the audio path, so the instrument keeps the
        original 303 saw/square voice.
    */
    class CyberWavetable3D : public juce::Component,
                             private juce::OpenGLRenderer,
                             private juce::Timer
    {
    public:
        CyberWavetable3D();
        ~CyberWavetable3D() override;

        /** Source of samples and note state. Passing nullptr shows the idle
            hologram. */
        void setTap (const ScopeTap* tap) noexcept { scopeTap = tap; }

        void setExpanded (bool shouldBeExpanded) noexcept;
        bool isExpanded() const noexcept { return expanded; }

        /** True once a GL context is rendering the scene. */
        bool isUsingGPU() const noexcept { return gpuActive.load(); }

        /** Forces the software renderer. Used by the headless tests and the
            snapshot tool so a still is deterministic on every machine. */
        static void setSoftwareRenderingForced (bool shouldForce);
        static bool isSoftwareRenderingForced() noexcept;

        /** Fired when the user clicks the scope (the editor toggles the size). */
        std::function<void()> onToggleRequested;

        //==========================================================================
        void paint (juce::Graphics&) override;
        void resized() override;
        void parentHierarchyChanged() override;
        void visibilityChanged() override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

    private:
        //======================================================================
        /** One perspective slice of the scene, already projected to pixels. */
        struct SceneRow
        {
            std::vector<juce::Point<float>> points;
            float depth = 0.0f;     // 0 = horizon, 1 = nearest
            float energy = 0.0f;    // rms of the slice, drives the glow
            float baseline = 0.0f;  // y of the slice centre, for the reflection
        };

        struct Scene
        {
            std::vector<SceneRow> rows;
            std::vector<std::pair<juce::Point<float>, juce::Point<float>>> gridRays;
            std::vector<std::pair<juce::Point<float>, juce::Point<float>>> gridRungs;
            juce::Rectangle<float> plot;
            float horizon = 0.0f;
            float scanY = 0.0f;
            bool live = false;
            int signatureIndex = 0;
            juce::String waveName;
            juce::String signatureName;
            float frequency = 110.0f;
        };

        Scene buildScene (juce::Rectangle<float> area, double nowSeconds) const;
        void drawSceneSoftware (juce::Graphics&, const Scene&) const;
        void drawSceneGL (const Scene&);
        void drawChrome (juce::Graphics&, const Scene&) const;

        //======================================================================
        // OpenGL plumbing
        void newOpenGLContextCreated() override;
        void renderOpenGL() override;
        void openGLContextClosing() override;

        void attachGL();
        void detachGL();
        void fallBackToSoftware();

        void timerCallback() override;

        const ScopeTap* scopeTap = nullptr;

        juce::OpenGLContext glContext;
        std::atomic<bool> gpuActive { false };
        std::atomic<int> cachedWidth { 0 };
        std::atomic<int> cachedHeight { 0 };

        bool glFailed = false;
        bool expanded = false;
        bool hovering = false;
        int framesSinceAttach = 0;
        int framesSinceHud = 0;

        Scene softwareScene;
        Scene glScene;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CyberWavetable3D)
    };
}
