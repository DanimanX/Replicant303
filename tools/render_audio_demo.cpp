//==============================================================================
// Renders the hard-techno acid demo used by the promo video.
//
//   render_audio_demo [output.wav]
//
// There is no host here: the pattern clock runs on the plugin's internal sync
// mode, patterns are switched on section boundaries and the filter / drive are
// automated per block. Everything you hear comes out of the actual DSP path.
//==============================================================================

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

#include <iostream>
#include <vector>

namespace
{
    constexpr double kSampleRate = 48000.0;
    constexpr int    kBlockSize  = 512;
    constexpr double kDuration   = 69.0;

    void setParam (Replicant303AudioProcessor& processor, const juce::String& id, float value)
    {
        if (auto* param = processor.apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    float pulse (double t, double period, double phase = 0.0)
    {
        return 0.5f + 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * t / period + phase);
    }
}

int main (int argc, char* argv[])
{
    const juce::File output = (argc > 1)
        ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
        : juce::File::getCurrentWorkingDirectory().getChildFile ("assets/replicant303_hardtechno_demo.wav");

    juce::ScopedJuceInitialiser_GUI gui;

    Replicant303AudioProcessor processor;
    processor.prepareToPlay (kSampleRate, kBlockSize);

    auto& sequencer = processor.getSequencer();
    sequencer.setSyncMode (Replicant303::PatternSequencer::SyncMode::Internal);
    sequencer.setTempo (138.0);
    sequencer.setSwing (0.12f);
    sequencer.setCurrentPattern (0);

    setParam (processor, "tempo", 138.0f);
    setParam (processor, "waveform", 0.0f);        // saw
    setParam (processor, "tuning", 0.0f);
    setParam (processor, "resonance", 0.80f);
    setParam (processor, "env_mod", 0.62f);
    setParam (processor, "decay", 0.55f);
    setParam (processor, "accent", 0.82f);
    setParam (processor, "volume", -4.0f);
    setParam (processor, "dist_active", 1.0f);
    setParam (processor, "dist_type", 0.0f);       // Poco Rat
    setParam (processor, "dist_drive", 0.55f);
    setParam (processor, "dist_tone", 0.58f);
    setParam (processor, "dist_mix", 0.92f);

    sequencer.start();

    const int numBlocks = (int) std::ceil (kDuration * kSampleRate / kBlockSize);
    juce::AudioBuffer<float> song (2, numBlocks * kBlockSize);
    song.clear();

    juce::AudioBuffer<float> block (2, kBlockSize);

    int currentSection = -1;

    for (int n = 0; n < numBlocks; ++n)
    {
        const double t = n * kBlockSize / kSampleRate;

        // ---------------------------------------------------------------------
        // Arrangement: four sections + a short outro.
        // ---------------------------------------------------------------------
        int section = 0;
        if (t >= 8.0)  section = 1;
        if (t >= 24.0) section = 2;
        if (t >= 40.0) section = 3;
        if (t >= 56.0) section = 4;
        if (t >= 64.0) section = 5;

        if (section != currentSection)
        {
            currentSection = section;

            switch (section)
            {
                case 0: sequencer.setCurrentPattern (0); break;   // Amber Line
                case 1: sequencer.setCurrentPattern (0); break;
                case 2: sequencer.setCurrentPattern (7); break;   // Last Replicant
                case 3: sequencer.setCurrentPattern (2); break;   // Neon Run
                case 4: sequencer.setCurrentPattern (7); break;
                case 5: sequencer.setCurrentPattern (1); break;   // Sub Level
                default: break;
            }
        }

        // ---------------------------------------------------------------------
        // Filter / drive automation.
        // ---------------------------------------------------------------------
        const float breathe = pulse (t, 8.0, -juce::MathConstants<double>::halfPi);
        float cutoff = 150.0f * std::pow (12.0f, breathe);

        if (section == 1) cutoff *= 1.25f;
        if (section == 2) cutoff *= 1.45f;
        if (section == 3) cutoff *= 1.75f;
        if (section == 4) cutoff *= 2.10f;
        if (section == 5) cutoff *= 0.55f;

        setParam (processor, "cutoff", juce::jlimit (40.0f, 4200.0f, cutoff));
        setParam (processor, "resonance", juce::jlimit (0.5f, 0.93f,
                   0.76f + 0.12f * pulse (t, 16.0) + (section >= 3 ? 0.04f : 0.0f)));
        setParam (processor, "env_mod", juce::jlimit (0.2f, 1.0f, 0.52f + 0.28f * breathe));
        setParam (processor, "decay", juce::jlimit (0.2f, 2.0f,
                   0.40f + 0.30f * pulse (t, 8.0, 1.2) + (section >= 2 ? 0.12f : 0.0f)));
        setParam (processor, "dist_drive", juce::jlimit (0.0f, 1.0f,
                   0.34f + 0.16f * (float) section + 0.10f * pulse (t, 4.0)));
        setParam (processor, "dist_tone", juce::jlimit (0.0f, 1.0f,
                   0.46f + 0.20f * pulse (t, 8.0, 0.9)));

        // Shape changes: square in the sub section, back to saw afterwards.
        setParam (processor, "waveform", (section == 3) ? 1.0f : 0.0f);

        // ---------------------------------------------------------------------
        block.clear();
        juce::MidiBuffer midi;
        processor.processBlock (block, midi);

        for (int ch = 0; ch < 2; ++ch)
            song.copyFrom (ch, n * kBlockSize, block, ch, 0, kBlockSize);
    }

    // -------------------------------------------------------------------------
    // Gentle limiting + peak normalisation.
    // -------------------------------------------------------------------------
    const int numSamples = numBlocks * kBlockSize;

    float peak = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < numSamples; ++i)
            peak = juce::jmax (peak, std::abs (song.getSample (ch, i)));

    // Keep the dynamics intact: scale to a safe peak instead of squashing the
    // whole render through a limiter (the mix stage adds the drums and master).
    const float normaliseGain = peak > 1.0e-6f ? 0.72f / peak : 1.0f;
    song.applyGain (normaliseGain);

    std::cout << "[INFO] Raw peak " << peak << ", normalised by " << normaliseGain << std::endl;

    // A short fade so the render starts and ends cleanly.
    const int fade = (int) (0.02 * kSampleRate);
    for (int ch = 0; ch < 2; ++ch)
    {
        song.applyGainRamp (ch, 0, fade, 0.0f, 1.0f);
        song.applyGainRamp (ch, numSamples - fade, fade, 1.0f, 0.0f);
    }

    if (! output.getParentDirectory().createDirectory())
        std::cerr << "[WARN] Could not create output directory" << std::endl;

    output.deleteFile();

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream (output.createOutputStream());

    if (stream == nullptr)
    {
        std::cerr << "[ERROR] Could not open " << output.getFullPathName() << std::endl;
        return 1;
    }

    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (stream.get(), kSampleRate, 2, 24, {}, 0));

    if (writer == nullptr)
    {
        std::cerr << "[ERROR] Could not create a WAV writer" << std::endl;
        return 1;
    }

    stream.release(); // the writer owns the stream now

    if (! writer->writeFromAudioSampleBuffer (song, 0, numSamples))
    {
        std::cerr << "[ERROR] WAV write failed" << std::endl;
        return 1;
    }

    writer.reset();

    std::cout << "[SUCCESS] Wrote " << (kDuration) << " s of acid to "
              << output.getFullPathName() << std::endl;

    return 0;
}
