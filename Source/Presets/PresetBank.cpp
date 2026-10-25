#include "PresetBank.h"

#include <cmath>

namespace Replicant303
{
    namespace
    {
        //==========================================================================
        // Name material. The factory lines keep the names they shipped with; the
        // generated slots use an adjective/noun table in the same two-word style.
        //==========================================================================
        const char* const kBankLetters = "ABCDEFGH";

        const char* const kBankNames[] =
        {
            "AMBER", "SUB", "NEON", "ACID", "BLADE", "ROOFTOP", "OFF WORLD", "LAST"
        };

        const char* const kFactoryNames[] =
        {
            "Amber Line", "Sub Level", "Neon Run", "Acid Rain",
            "Blade Pulse", "Rooftop Chase", "Off World", "Last Replicant"
        };

        const char* const kAdjectives[] =
        {
            "Amber", "Sub", "Neon", "Acid", "Blade", "Rooftop", "Off", "Last",
            "Chrome", "Static", "Velvet", "Rust", "Ghost", "Silver", "Crimson",
            "Electric", "Hollow", "Bright", "Dark", "Fever", "Glass", "Iron",
            "Lunar", "Magnetic", "Midnight", "Mirror", "Molten", "Obsidian",
            "Phantom", "Plastic", "Prism", "Quartz", "Radiant", "Reactor",
            "Rebel", "Redline", "Satin", "Scarlet", "Secret", "Shadow", "Silent",
            "Solar", "Sonic", "Spectral", "Steel", "Storm", "Subzero", "Sunken",
            "Tidal", "Toxic", "Turbo", "Twilight", "Ultraviolet", "Vacuum",
            "Vapor", "Velour", "Violet", "Voltage", "Wired", "Wraith", "Zero",
            "Zenith", "Cobalt", "Carbon"
        };

        const char* const kNouns[] =
        {
            "Line", "Level", "Run", "Rain", "Pulse", "Chase", "World",
            "Replicant", "Circuit", "Signal", "Engine", "Wire", "Grid", "Vault",
            "Machine", "Protocol", "Sequence", "District", "Corridor",
            "Frequency", "Reactor", "Tempo", "Voltage", "Current", "Motion",
            "Shadow", "Mirror", "Ladder", "Filter", "Oscillator", "Groove",
            "Pattern", "Sector", "Tunnel", "Chamber", "Beacon", "Antenna",
            "Archive", "Bishop", "Cipher", "Column", "Compass", "Console",
            "Cortex", "Cradle", "Cursor", "Delta", "Divider", "Driver", "Echo",
            "Ember", "Envelope", "Fabric", "Factory", "Frame", "Horizon",
            "Kernel", "Lantern", "Lattice", "Matrix", "Meridian", "Monolith",
            "Neuron", "Orbit"
        };

        static_assert (sizeof (kAdjectives) / sizeof (kAdjectives[0]) == 64);
        static_assert (sizeof (kNouns) / sizeof (kNouns[0]) == 64);

        /** Musical material per bank. Every line stays inside C1..C3. */
        const int kScales[PresetBank::numBanks][8] =
        {
            { 0, 3, 5, 7, 10, 12, 15, 19 },   // A minor pentatonic
            { 0, 2, 3, 5, 7, 8, 10, 12 },     // B natural minor
            { 0, 2, 4, 7, 9, 12, 14, 16 },    // C major pentatonic
            { 0, 1, 3, 5, 6, 8, 10, 12 },     // D phrygian
            { 0, 3, 5, 6, 7, 10, 12, 15 },    // E blues
            { 0, 2, 3, 5, 7, 9, 10, 12 },     // F dorian
            { 0, 2, 3, 5, 7, 8, 11, 12 },     // G harmonic minor
            { 0, 1, 2, 4, 6, 7, 10, 12 }      // H chromatic acid
        };

        const int kLengths[] = { 16, 16, 16, 16, 16, 12, 8, 7 };

        //==========================================================================
        /** Small deterministic PRNG.

            juce::Random is deliberately not used here: the factory bank and the
            checked-in .midi examples must be byte-for-byte reproducible across
            platforms and JUCE upgrades. SplitMix32 gives us that guarantee with
            a handful of lines.
        */
        struct FastRandom
        {
            explicit FastRandom (juce::uint32 seedValue) noexcept
                : state (seedValue != 0 ? seedValue : 0x30303030u)
            {
            }

            juce::uint32 next() noexcept
            {
                state += 0x9e3779b9u;
                auto z = state;
                z = (z ^ (z >> 16)) * 0x85ebca6bu;
                z = (z ^ (z >> 13)) * 0xc2b2ae35u;
                return z ^ (z >> 16);
            }

            float nextFloat() noexcept
            {
                return (float) (next() >> 8) * (1.0f / 16777216.0f);
            }

            int nextInt (int low, int high) noexcept
            {
                const auto span = (juce::uint32) (high - low + 1);
                return low + (int) (next() % span);
            }

            float between (float low, float high) noexcept
            {
                return low + (high - low) * nextFloat();
            }

            bool chance (float probability) noexcept
            {
                return nextFloat() < probability;
            }

            juce::uint32 state;
        };

        juce::uint32 makeSeed (int bank, int slot, juce::uint32 seed) noexcept
        {
            return seed
                 ^ ((juce::uint32) (bank + 1) * 0x9e3779b9u)
                 ^ ((juce::uint32) (slot + 1) * 0x85ebca6bu)
                 ^ 0x52443303u;   // "RD303"
        }

        float logBetween (FastRandom& rng, float low, float high) noexcept
        {
            return std::exp (rng.between (std::log (low), std::log (high)));
        }

        bool isFactoryName (const juce::String& name)
        {
            for (const auto* factoryName : kFactoryNames)
                if (name == factoryName)
                    return true;

            return false;
        }

        juce::String makeGeneratedName (int bank, int slot)
        {
            // Both multipliers are odd, so for a fixed bank the 64 slots walk
            // the two 64-entry tables without ever repeating a pair.
            const auto adjective = (int) (((juce::uint32) slot * 7u
                                         + (juce::uint32) bank * 11u) % 64u);
            const auto noun = (int) (((juce::uint32) slot * 17u
                                    + (juce::uint32) bank * 23u) % 64u);

            auto name = juce::String (kAdjectives[adjective]) + " " + kNouns[noun];

            if (isFactoryName (name))
                name += " II";

            return name;
        }

        bool almostEqual (float a, float b) noexcept
        {
            return std::abs (a - b) < 1.0e-4f;
        }
    }

    //==============================================================================
    void Preset::sanitise() noexcept
    {
        name = name.trim();

        if (name.isEmpty())
            name = "Untitled";

        if (name.length() > 64)
            name = name.substring (0, 64);

        pattern.sanitise();

        waveform  = juce::jlimit (0, 1, waveform);
        tuning    = juce::jlimit (-12.0f, 12.0f, tuning);
        cutoff    = juce::jlimit (25.0f, 4500.0f, cutoff);
        resonance = juce::jlimit (0.0f, 1.0f, resonance);
        envMod    = juce::jlimit (0.0f, 1.0f, envMod);
        decay     = juce::jlimit (0.20f, 2.50f, decay);
        accent    = juce::jlimit (0.0f, 1.0f, accent);
        tempo     = juce::jlimit (40.0f, 300.0f, tempo);
        swing     = juce::jlimit (0.0f, 0.66f, swing);

        distType  = juce::jlimit (0, 4, distType);
        distDrive = juce::jlimit (0.0f, 1.0f, distDrive);
        distTone  = juce::jlimit (0.0f, 1.0f, distTone);
        distMix   = juce::jlimit (0.0f, 1.0f, distMix);
    }

    bool Preset::operator== (const Preset& other) const noexcept
    {
        return name == other.name
            && pattern == other.pattern
            && waveform == other.waveform
            && almostEqual (tuning, other.tuning)
            && almostEqual (cutoff, other.cutoff)
            && almostEqual (resonance, other.resonance)
            && almostEqual (envMod, other.envMod)
            && almostEqual (decay, other.decay)
            && almostEqual (accent, other.accent)
            && almostEqual (tempo, other.tempo)
            && almostEqual (swing, other.swing)
            && distActive == other.distActive
            && distType == other.distType
            && almostEqual (distDrive, other.distDrive)
            && almostEqual (distTone, other.distTone)
            && almostEqual (distMix, other.distMix);
    }

    juce::ValueTree Preset::toValueTree() const
    {
        juce::ValueTree tree ("PRESET");
        tree.setProperty ("name", name, nullptr);
        tree.setProperty ("waveform", waveform, nullptr);
        tree.setProperty ("tuning", (double) tuning, nullptr);
        tree.setProperty ("cutoff", (double) cutoff, nullptr);
        tree.setProperty ("resonance", (double) resonance, nullptr);
        tree.setProperty ("envMod", (double) envMod, nullptr);
        tree.setProperty ("decay", (double) decay, nullptr);
        tree.setProperty ("accent", (double) accent, nullptr);
        tree.setProperty ("tempo", (double) tempo, nullptr);
        tree.setProperty ("swing", (double) swing, nullptr);
        tree.setProperty ("distActive", distActive, nullptr);
        tree.setProperty ("distType", distType, nullptr);
        tree.setProperty ("distDrive", (double) distDrive, nullptr);
        tree.setProperty ("distTone", (double) distTone, nullptr);
        tree.setProperty ("distMix", (double) distMix, nullptr);
        tree.appendChild (pattern.toValueTree(), nullptr);
        return tree;
    }

    Preset Preset::fromValueTree (const juce::ValueTree& tree)
    {
        Preset preset;

        if (! tree.isValid() || ! tree.hasType ("PRESET"))
            return preset;

        preset.name = tree.getProperty ("name", "Untitled").toString();
        preset.waveform = (int) tree.getProperty ("waveform", 0);
        preset.tuning = (float) (double) tree.getProperty ("tuning", 0.0);
        preset.cutoff = (float) (double) tree.getProperty ("cutoff", 350.0);
        preset.resonance = (float) (double) tree.getProperty ("resonance", 0.65);
        preset.envMod = (float) (double) tree.getProperty ("envMod", 0.60);
        preset.decay = (float) (double) tree.getProperty ("decay", 0.75);
        preset.accent = (float) (double) tree.getProperty ("accent", 0.70);
        preset.tempo = (float) (double) tree.getProperty ("tempo", 125.0);
        preset.swing = (float) (double) tree.getProperty ("swing", 0.0);
        preset.distActive = (bool) tree.getProperty ("distActive", true);
        preset.distType = (int) tree.getProperty ("distType", 0);
        preset.distDrive = (float) (double) tree.getProperty ("distDrive", 0.50);
        preset.distTone = (float) (double) tree.getProperty ("distTone", 0.60);
        preset.distMix = (float) (double) tree.getProperty ("distMix", 0.85);

        preset.pattern = Pattern::fromValueTree (tree.getChildWithName ("PATTERN"));
        preset.sanitise();
        return preset;
    }

    //==============================================================================
    PresetBank::PresetBank()
    {
        for (int bank = 0; bank < numBanks; ++bank)
            for (int slot = 0; slot < presetsPerBank; ++slot)
                presets[(size_t) bank][(size_t) slot] = makeFactoryPreset (bank, slot);
    }

    juce::String PresetBank::getBankLetter (int bank)
    {
        return juce::String::charToString (kBankLetters[juce::jlimit (0, numBanks - 1, bank)]);
    }

    juce::String PresetBank::getBankName (int bank)
    {
        return kBankNames[juce::jlimit (0, numBanks - 1, bank)];
    }

    Preset PresetBank::getPreset (int bank, int slot) const
    {
        const juce::ScopedLock scopedLock (lock);
        return presets[(size_t) juce::jlimit (0, numBanks - 1, bank)]
                      [(size_t) juce::jlimit (0, presetsPerBank - 1, slot)];
    }

    void PresetBank::setPreset (int bank, int slot, const Preset& preset)
    {
        const int b = juce::jlimit (0, numBanks - 1, bank);
        const int s = juce::jlimit (0, presetsPerBank - 1, slot);

        auto copy = preset;
        copy.sanitise();

        const juce::ScopedLock scopedLock (lock);
        presets[(size_t) b][(size_t) s] = copy;
        overridden[(size_t) b][(size_t) s] = true;
    }

    void PresetBank::resetPreset (int bank, int slot)
    {
        const int b = juce::jlimit (0, numBanks - 1, bank);
        const int s = juce::jlimit (0, presetsPerBank - 1, slot);

        const juce::ScopedLock scopedLock (lock);
        presets[(size_t) b][(size_t) s] = makeFactoryPreset (b, s);
        overridden[(size_t) b][(size_t) s] = false;
    }

    bool PresetBank::isOverridden (int bank, int slot) const noexcept
    {
        const juce::ScopedLock scopedLock (lock);
        return overridden[(size_t) juce::jlimit (0, numBanks - 1, bank)]
                         [(size_t) juce::jlimit (0, presetsPerBank - 1, slot)];
    }

    int PresetBank::getSelectedSlot (int bank) const noexcept
    {
        const juce::ScopedLock scopedLock (lock);
        return selectedSlots[(size_t) juce::jlimit (0, numBanks - 1, bank)];
    }

    void PresetBank::setSelectedSlot (int bank, int slot) noexcept
    {
        const juce::ScopedLock scopedLock (lock);
        selectedSlots[(size_t) juce::jlimit (0, numBanks - 1, bank)]
            = juce::jlimit (0, presetsPerBank - 1, slot);
    }

    //==============================================================================
    Preset PresetBank::makeFactoryPreset (int bank, int slot)
    {
        const int b = juce::jlimit (0, numBanks - 1, bank);
        const int s = juce::jlimit (0, presetsPerBank - 1, slot);

        if (s == 0)
        {
            // The eight lines that predate the vault stay exactly as they were.
            Preset preset;
            preset.name = kFactoryNames[b];
            preset.pattern = makeDefaultPattern (b);
            preset.sanitise();
            return preset;
        }

        return generatePreset (b, s, 0);
    }

    Preset PresetBank::generatePreset (int bank, int slot, juce::uint32 seed)
    {
        const int b = juce::jlimit (0, numBanks - 1, bank);
        const int s = juce::jlimit (0, presetsPerBank - 1, slot);

        FastRandom rng (makeSeed (b, s, seed));

        Preset preset;
        preset.name = makeGeneratedName (b, s);

        if (s == 0)
            preset.name = juce::String (kFactoryNames[b]) + " "
                          + juce::String (1 + (int) (seed % 9u));

        //----------------------------------------------------------------------
        // Acid line
        //----------------------------------------------------------------------
        const auto& scale = kScales[b];
        const int root = 24 + rng.nextInt (0, 12);
        const int length = kLengths[rng.nextInt (0, (int) (sizeof (kLengths) / sizeof (kLengths[0])) - 1)];

        preset.pattern.length = length;

        const float density = rng.between (0.62f, 0.94f);
        const float accentChance = rng.between (0.16f, 0.38f);
        const float slideChance = rng.between (0.10f, 0.32f);

        int previousNote = root + scale[0];

        for (int i = 0; i < Pattern::maxSteps; ++i)
        {
            auto& step = preset.pattern[(size_t) i];

            step.gate = (i == 0) || rng.chance (density);

            const int degree = rng.nextInt (0, 7);
            int octave = 0;

            if (rng.chance (0.16f)) octave = 12;
            if (rng.chance (0.08f)) octave = -12;

            int note = root + scale[degree] + octave;

            // Repeated notes are the backbone of a 303 line; they also give
            // slides something meaningful to glide from.
            if (rng.chance (0.12f))
                note = previousNote;

            step.note = juce::jlimit (Pattern::lowestNote, Pattern::highestNote, note);
            previousNote = step.note;

            step.accent = step.gate && rng.chance (accentChance);
            step.slide = step.gate && rng.chance (slideChance);
        }

        // A generated line must always have enough material to be a pattern.
        int gates = 0;
        for (int i = 0; i < length; ++i)
            if (preset.pattern[i].gate)
                ++gates;

        for (int guard = 0; guard < 64 && gates < 4; ++guard)
        {
            const int i = rng.nextInt (0, length - 1);
            if (! preset.pattern[i].gate)
            {
                preset.pattern[i].gate = true;
                ++gates;
            }
        }

        bool allSameNote = true;
        for (int i = 1; i < length; ++i)
            if (preset.pattern[i].note != preset.pattern[0].note)
                allSameNote = false;

        if (allSameNote)
        {
            preset.pattern[length / 2].note = juce::jlimit (
                Pattern::lowestNote, Pattern::highestNote,
                preset.pattern[0].note + scale[3]);
            preset.pattern[length - 1].note = juce::jlimit (
                Pattern::lowestNote, Pattern::highestNote,
                preset.pattern[0].note + scale[5]);
        }

        bool anyAccent = false;
        for (int i = 0; i < length; ++i)
            anyAccent = anyAccent || preset.pattern[i].accent;

        if (! anyAccent)
            preset.pattern[0].accent = true;

        //----------------------------------------------------------------------
        // Voice archetypes. The bank/slot pair walks eight distinct characters
        // so a bank never feels like 64 variations of the same patch.
        //----------------------------------------------------------------------
        const int archetype = (b + s) % 8;

        switch (archetype)
        {
            case 0: // Classic acid
                preset.waveform = 0;
                preset.cutoff = logBetween (rng, 170.0f, 900.0f);
                preset.resonance = rng.between (0.55f, 0.84f);
                preset.envMod = rng.between (0.50f, 0.82f);
                preset.decay = rng.between (0.38f, 0.90f);
                preset.accent = rng.between (0.60f, 0.85f);
                preset.distActive = true;
                preset.distType = 0;
                preset.distDrive = rng.between (0.28f, 0.62f);
                preset.distTone = rng.between (0.45f, 0.75f);
                preset.distMix = rng.between (0.62f, 0.90f);
                break;

            case 1: // Sub pressure
                preset.waveform = 0;
                preset.cutoff = logBetween (rng, 70.0f, 380.0f);
                preset.resonance = rng.between (0.30f, 0.68f);
                preset.envMod = rng.between (0.30f, 0.68f);
                preset.decay = rng.between (0.65f, 1.45f);
                preset.accent = rng.between (0.55f, 0.82f);
                preset.distActive = true;
                preset.distType = rng.chance (0.70f) ? 0 : 2;
                preset.distDrive = rng.between (0.18f, 0.48f);
                preset.distTone = rng.between (0.25f, 0.55f);
                preset.distMix = rng.between (0.55f, 0.82f);
                break;

            case 2: // Bright / open
                preset.waveform = rng.chance (0.75f) ? 1 : 0;
                preset.cutoff = logBetween (rng, 800.0f, 2600.0f);
                preset.resonance = rng.between (0.35f, 0.78f);
                preset.envMod = rng.between (0.68f, 0.95f);
                preset.decay = rng.between (0.20f, 0.52f);
                preset.accent = rng.between (0.50f, 0.80f);
                preset.distActive = true;
                preset.distType = rng.chance (0.65f) ? 1 : 0;
                preset.distDrive = rng.between (0.15f, 0.42f);
                preset.distTone = rng.between (0.62f, 0.90f);
                preset.distMix = rng.between (0.35f, 0.68f);
                break;

            case 3: // Screamer
                preset.waveform = 0;
                preset.cutoff = logBetween (rng, 280.0f, 1300.0f);
                preset.resonance = rng.between (0.68f, 0.94f);
                preset.envMod = rng.between (0.58f, 0.90f);
                preset.decay = rng.between (0.22f, 0.62f);
                preset.accent = rng.between (0.65f, 0.92f);
                preset.distActive = true;
                preset.distType = rng.chance (0.60f) ? 2 : 0;
                preset.distDrive = rng.between (0.58f, 0.92f);
                preset.distTone = rng.between (0.40f, 0.78f);
                preset.distMix = rng.between (0.72f, 1.0f);
                break;

            case 4: // Germanium fuzz
                preset.waveform = rng.chance (0.70f) ? 1 : 0;
                preset.cutoff = logBetween (rng, 200.0f, 1000.0f);
                preset.resonance = rng.between (0.45f, 0.80f);
                preset.envMod = rng.between (0.48f, 0.86f);
                preset.decay = rng.between (0.30f, 0.85f);
                preset.accent = rng.between (0.55f, 0.85f);
                preset.distActive = true;
                preset.distType = 3;
                preset.distDrive = rng.between (0.48f, 0.90f);
                preset.distTone = rng.between (0.30f, 0.68f);
                preset.distMix = rng.between (0.60f, 0.95f);
                break;

            case 5: // Wavefolder
                preset.waveform = 0;
                preset.cutoff = logBetween (rng, 380.0f, 1900.0f);
                preset.resonance = rng.between (0.42f, 0.82f);
                preset.envMod = rng.between (0.62f, 0.94f);
                preset.decay = rng.between (0.25f, 0.72f);
                preset.accent = rng.between (0.55f, 0.88f);
                preset.distActive = true;
                preset.distType = 4;
                preset.distDrive = rng.between (0.38f, 0.86f);
                preset.distTone = rng.between (0.42f, 0.82f);
                preset.distMix = rng.between (0.50f, 0.90f);
                break;

            case 6: // Clean / dry
                preset.waveform = rng.chance (0.55f) ? 0 : 1;
                preset.cutoff = logBetween (rng, 650.0f, 2200.0f);
                preset.resonance = rng.between (0.18f, 0.55f);
                preset.envMod = rng.between (0.38f, 0.78f);
                preset.decay = rng.between (0.28f, 0.72f);
                preset.accent = rng.between (0.42f, 0.75f);
                preset.distActive = rng.chance (0.35f);
                preset.distType = rng.nextInt (0, 4);
                preset.distDrive = rng.between (0.10f, 0.38f);
                preset.distTone = rng.between (0.50f, 0.85f);
                preset.distMix = rng.between (0.20f, 0.55f);
                break;

            default: // Hard / industrial
                preset.waveform = 0;
                preset.cutoff = logBetween (rng, 140.0f, 720.0f);
                preset.resonance = rng.between (0.60f, 0.94f);
                preset.envMod = rng.between (0.70f, 0.97f);
                preset.decay = rng.between (0.20f, 0.48f);
                preset.accent = rng.between (0.72f, 0.96f);
                preset.distActive = true;
                preset.distType = rng.chance (0.68f) ? 0 : 2;
                preset.distDrive = rng.between (0.70f, 1.0f);
                preset.distTone = rng.between (0.32f, 0.70f);
                preset.distMix = rng.between (0.78f, 1.0f);
                break;
        }

        preset.tempo = std::round (rng.between (116.0f, 150.0f) * 2.0f) * 0.5f;
        preset.swing = rng.chance (0.55f) ? 0.0f : rng.between (0.04f, 0.30f);
        preset.tuning = rng.chance (0.10f) ? (rng.chance (0.5f) ? -12.0f : 12.0f) : 0.0f;

        preset.sanitise();
        return preset;
    }

    //==============================================================================
    juce::ValueTree PresetBank::toValueTree() const
    {
        const juce::ScopedLock scopedLock (lock);

        juce::ValueTree root ("PRESETBANK");
        root.setProperty ("version", 1, nullptr);

        for (int bank = 0; bank < numBanks; ++bank)
            root.setProperty (juce::String ("selected") + juce::String (bank), selectedSlots[(size_t) bank], nullptr);

        for (int bank = 0; bank < numBanks; ++bank)
        {
            for (int slot = 0; slot < presetsPerBank; ++slot)
            {
                if (! overridden[(size_t) bank][(size_t) slot])
                    continue;

                auto presetTree = presets[(size_t) bank][(size_t) slot].toValueTree();
                presetTree.setProperty ("bank", bank, nullptr);
                presetTree.setProperty ("slot", slot, nullptr);
                root.appendChild (presetTree, nullptr);
            }
        }

        return root;
    }

    void PresetBank::restoreFromValueTree (const juce::ValueTree& tree)
    {
        const juce::ScopedLock scopedLock (lock);

        for (int bank = 0; bank < numBanks; ++bank)
        {
            for (int slot = 0; slot < presetsPerBank; ++slot)
            {
                presets[(size_t) bank][(size_t) slot] = makeFactoryPreset (bank, slot);
                overridden[(size_t) bank][(size_t) slot] = false;
            }

            selectedSlots[(size_t) bank] = 0;
        }

        if (! tree.isValid() || ! tree.hasType ("PRESETBANK"))
            return;

        for (int bank = 0; bank < numBanks; ++bank)
            selectedSlots[(size_t) bank] = juce::jlimit (
                0, presetsPerBank - 1,
                (int) tree.getProperty (juce::String ("selected") + juce::String (bank), 0));

        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            const auto child = tree.getChild (i);

            if (! child.hasType ("PRESET"))
                continue;

            const int bank = (int) child.getProperty ("bank", -1);
            const int slot = (int) child.getProperty ("slot", -1);

            if (! juce::isPositiveAndBelow (bank, numBanks)
                 || ! juce::isPositiveAndBelow (slot, presetsPerBank))
                continue;

            presets[(size_t) bank][(size_t) slot] = Preset::fromValueTree (child);
            overridden[(size_t) bank][(size_t) slot] = true;
        }
    }
}
