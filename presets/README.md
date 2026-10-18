# Replicant303 preset examples

This folder ships the eight original factory patterns as portable `.midi`
presets, one per pattern letter:

```
presets/
├── A/01_Amber_Line.midi
├── B/01_Sub_Level.midi
├── C/01_Neon_Run.midi
├── D/01_Acid_Rain.midi
├── E/01_Blade_Pulse.midi
├── F/01_Rooftop_Chase.midi
├── G/01_Off_World.midi
└── H/01_Last_Replicant.midi
```

Each file is a Standard MIDI File that any DAW can play (16th grid, accents as
velocity 110, slides as full-length notes). It also contains an
`R303PRESET/1:` text meta-event with the complete Replicant303 patch, so the
plugin can import it with the original voice, distortion, swing and tempo
intact.

## Regenerating

The files are produced by the same `PresetBank` generator the plugin uses:

```bash
ninja -C build generate_presets

./build/generate_presets          # the eight examples above
./build/generate_presets --all    # the full 8 x 64 factory bank
```

The generator is deterministic, so re-running it produces byte-identical
files. CI regenerates the examples and fails if they drift from the code.

## Using the presets

Open the **PATTERN** drawer, choose the bank letter with the `A`–`H` slots,
then pick a page (`1`–`8`) and a slot (`01`–`08`). The `PRESET` row loads,
stores, generates, imports and exports presets without leaving the panel.
