#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>

namespace Replicant303
{
    //==========================================================================
    /** Lock-free, best-effort audio tap for the 3D wavetable scope.

        The audio thread only ever writes one float and bumps an index; the UI
        thread copies the newest window whenever it wants to draw. A tear at the
        block boundary would show up as one slightly stale sample in a visual,
        which is why no lock (and no allocation) is allowed on the audio side.

        The tap also carries the note state the scope needs to label itself:
        which 303 waveform is running, at what frequency, and whether the
        envelope is still open.
    */
    class ScopeTap
    {
    public:
        static constexpr int capacity = 8192;
        static_assert ((capacity & (capacity - 1)) == 0, "capacity must be a power of two");

        void reset() noexcept
        {
            writeIndex.store (0, std::memory_order_release);
            amplitude.store (0.0f, std::memory_order_relaxed);
        }

        /** Audio thread: appends one oscillator sample. */
        void push (float sample) noexcept
        {
            const auto index = writeIndex.load (std::memory_order_relaxed);
            samples[(size_t) (index & (capacity - 1))] = sample;
            writeIndex.store (index + 1, std::memory_order_release);
        }

        /** Audio thread: publishes the voice state for the HUD. */
        void setNoteState (int waveformIndex, float frequencyHz, float envelope) noexcept
        {
            waveform.store (waveformIndex, std::memory_order_relaxed);
            frequency.store (frequencyHz, std::memory_order_relaxed);
            amplitude.store (envelope, std::memory_order_relaxed);
        }

        void setSampleRate (double newSampleRate) noexcept
        {
            sampleRate.store (newSampleRate > 0.0 ? newSampleRate : 44100.0,
                              std::memory_order_relaxed);
        }

        /** UI thread: copies the newest `numSamples` into `dest`, oldest first.

            Samples that predate the first write are zero-filled, so a scope can
            draw a flat line before the first note instead of garbage.
            @returns true when the buffer already holds that much history.
        */
        bool copyLatest (float* dest, int numSamples) const noexcept
        {
            if (dest == nullptr || numSamples <= 0)
                return false;

            numSamples = std::min (numSamples, capacity);

            const auto end = writeIndex.load (std::memory_order_acquire);
            const int start = end - numSamples;

            for (int i = 0; i < numSamples; ++i)
            {
                const int absolute = start + i;

                dest[i] = (absolute < 0)
                    ? 0.0f
                    : samples[(size_t) (absolute & (capacity - 1))];
            }

            return end >= numSamples;
        }

        int getWaveformIndex() const noexcept { return waveform.load (std::memory_order_relaxed); }
        float getFrequencyHz() const noexcept { return frequency.load (std::memory_order_relaxed); }
        float getAmplitude() const noexcept { return amplitude.load (std::memory_order_relaxed); }
        double getSampleRate() const noexcept { return sampleRate.load (std::memory_order_relaxed); }

        /** True while a note is sounding (envelope still open). */
        bool isLive() const noexcept { return getAmplitude() > 0.005f; }

    private:
        std::array<float, (size_t) capacity> samples {};
        std::atomic<int> writeIndex { 0 };
        std::atomic<int> waveform { 0 };
        std::atomic<float> frequency { 110.0f };
        std::atomic<float> amplitude { 0.0f };
        std::atomic<double> sampleRate { 44100.0 };
    };
}
