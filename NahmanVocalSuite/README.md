# NAHMAN VOCAL SUITE — v0.5 Auto-Tune update

JUCE C++ project configured for VST3 and Standalone.

## Added in v0.5
- Dedicated Auto-Tune / pitch-correction on/off switch.
- Musical key selection (C through B).
- Scale selection: Chromatic, Major, Minor.
- Retune Speed control.
- Correction Amount control.
- Rolling mono pitch tracker (autocorrelation) and a lightweight dual-tap, crossfaded delay pitch shifter for monophonic vocal material.
- All labels are Hebrew-first with compact English labels.

## Important limitations
This is an experimental, lightweight pitch-correction implementation, not a commercial-grade phase-vocoder or formant-preserving Auto-Tune. It is intended for modest correction on a single lead vocal. It may produce artifacts on fast passages, vibrato, breathy/rough vocals, polyphonic material, or large pitch shifts. The pitch detector is block-based and adds tracking lag. Test with a copy of the vocal and keep the amount moderate.

## Build
Requires CMake 3.22+ and an internet connection on first configure to fetch JUCE 8.0.6. Build with CMake for VST3 and Standalone targets.
