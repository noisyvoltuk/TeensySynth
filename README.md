# SynthTemplate — Teensy 4.0 + Audio Shield

A modular synthesizer template. Each "library" is a self-contained `.h` file.
Build a new synth by including the modules you need and writing a thin `.ino`.

---

## File layout

```
SynthTemplate/
├── SynthTemplate.h      ← orchestrator: audio shield, MIDI, helpers
├── Lib_Encoder.h        ← rotary encoders with push button
├── Lib_Display.h        ← 20×4 LCD or 128×64 OLED
├── Lib_VCO.h            ← AudioSynthWaveform wrapper
├── Lib_Filter.h         ← AudioFilterStateVariable wrapper
├── Lib_Envelope.h       ← AudioEffectEnvelope (ADSR) wrapper
└── MySynth.ino          ← example: monophonic subtractive synth
```

---

## How to make a new synth

1. Copy this folder and rename the `.ino` to `YourSynth.ino`
2. In `SynthTemplate.h`, comment out modules you don't need
3. Declare your audio objects + `AudioConnection` patch cords in the `.ino`
4. Wire MIDI callbacks and encoder/display updates in `setup()` / `loop()`

---

## Module quick reference

### VCO (Lib_VCO.h)
```cpp
VCO osc;
osc.begin(WAVE_SAW);
osc.noteOn(midiNote, velocityNorm);
osc.noteOff();
osc.setDetune(cents);       // −100 to +100 cents
osc.setShape(WAVE_SQUARE);
osc.setPulseWidth(0.3f);    // for WAVE_PULSE only
```

### VCFilter (Lib_Filter.h)
```cpp
VCFilter filt;
filt.begin();
filt.setCutoffNorm(0.5f);   // 0.0–1.0 logarithmic
filt.setResonanceNorm(0.3f);
filt.setEnvelopeAmount(0.8f);
filt.applyEnvelopeMod(envValue); // call from envelope callback
// Audio outputs: filt.svf — port 0=LP, 1=BP, 2=HP
```

### Envelope (Lib_Envelope.h)
```cpp
Envelope env;
env.begin(attack_ms, decay_ms, sustain_0to1, release_ms);
env.noteOn();
env.noteOff();
env.setAttackNorm(0.1f);  // normalised setters
```

### EncoderKnob (Lib_Encoder.h)
```cpp
EncoderKnob enc(pinA, pinB, pinButton);
enc.begin();
enc.update();          // call every loop()
enc.position();        // current absolute position
enc.delta();           // change since last update
enc.pressed();         // true on button press edge
```

### SynthDisplay (Lib_Display.h)
```cpp
SynthDisplay disp;     // defaults to 0x27 I2C LCD
disp.begin();
disp.header("MySynth");
disp.printParam(row, "Cutoff", "800Hz");
disp.print(row, "Any string");
```

---

## Required libraries (install via Arduino Library Manager)

| Library                   | Used by         |
|---------------------------|-----------------|
| Teensy Audio              | VCO, Filter, Env |
| MIDI Library              | SynthTemplate.h |
| Encoder                   | Lib_Encoder.h   |
| LiquidCrystal I2C         | Lib_Display.h   |
| Adafruit SSD1306 + GFX    | Lib_Display.h (OLED) |

---

## Audio memory

`AudioMemory(120)` is set in `SynthTemplate::init()`. Increase this if you add
many voices or effects and hear glitches. Each block is 128 samples × 2 bytes = 256 bytes.

---

## Ideas for additional modules

- `Lib_LFO.h` — `AudioSynthWaveformDc` + `AudioSynthWaveform` for modulation
- `Lib_Arpeggiator.h` — note queue stepped by `elapsedMillis`
- `Lib_Sequencer.h` — step sequencer with `elapsedMillis`
- `Lib_Chorus.h` — `AudioEffectChorus` wrapper
- `Lib_Reverb.h` — `AudioEffectReverb` or `AudioEffectFreeverb` wrapper
- `Lib_Delay.h` — `AudioEffectDelay` wrapper
- `Lib_Mixer.h` — named channel `AudioMixer4` wrapper
