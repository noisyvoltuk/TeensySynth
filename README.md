# BasicSynth — Teensy 4.0 + Audio Shield

A 2-VCO subtractive synth with LFO, filter, and dual ADSR, controlled via
two encoders and a 20x4 LCD.

## Signal flow

```
VCO1 ─┐
      ├─> Mixer ──> VCF (state-variable) ──> VCA ──> Output L/R
VCO2 ─┘                  ▲                    ▲
                          │                    │
LFO ──> (FM into VCO1/2)  │                    │
                    ADSR1 ┘              ADSR2 ┘
                  (filter env)         (amp env)
```

- **VCO1**: sine by default
- **VCO2**: sawtooth by default, with adjustable detune
- **LFO**: modulates pitch of both VCOs (vibrato) — rate and depth adjustable
- **VCF**: state-variable filter, cutoff/resonance adjustable, modulated by ADSR1
- **ADSR1**: filter envelope (cutoff modulation amount via "Filter Env Amt")
- **ADSR2**: amplitude envelope (controls VCA gain)
- **VCA**: final output stage

## Wiring

### Encoders
| Function | Pins (A, B, Button) |
|---|---|
| Encoder 1 — Parameter Select | 2, 3, 4 |
| Encoder 2 — Value Edit       | 5, 6, 7 |

Each encoder needs GND and 3.3V as well. Internal pullups are used for buttons.

### Display (20x4 LCD, I2C)
| LCD Pin | Teensy Pin |
|---|---|
| SDA | 18 |
| SCL | 19 |
| VCC | 5V or 3.3V (check your LCD board) |
| GND | GND |

Default I2C address: `0x27`. If your display doesn't show anything, try `0x3F`
(change in `BasicSynth.ino`: `LiquidCrystal_I2C lcd(0x3F, 20, 4);`)

### Audio Shield
See breadboard wiring table from earlier — I2C (18/19), SPI (11/12/13),
MEMCS (6), and I2S (7, 8, 20, 21, 23) all required.

## Controls

- **Encoder 1** (turn): scroll through the parameter list (cursor `>` shows selection)
- **Encoder 2** (turn): change the value of the currently selected parameter
- Display shows 4 parameters at a time, auto-scrolling to keep selection visible

## Parameters

| Parameter | Range | Notes |
|---|---|---|
| VCO1 Wave | Sine/Saw/Square/Tri/Pulse | |
| VCO2 Wave | Sine/Saw/Square/Tri/Pulse | |
| VCO2 Detune | -100 to +100 cents | relative to VCO1 |
| LFO Rate | 0.1 - 20 Hz | |
| LFO Depth | 0-100% | 0 = off |
| VCF Cutoff | 20-20000 Hz | |
| VCF Resonance | 0.7-5.0 Q | |
| ADSR1 Attack/Decay/Sustain/Release | filter envelope | |
| ADSR2 Attack/Decay/Sustain/Release | amplitude envelope | |
| Filter Env Amt | 0-100% | how much ADSR1 affects cutoff |

## MIDI

Connect via USB MIDI (compile with `USB_MIDI_SERIAL` to also get serial debug).
Sending a Note On triggers both envelopes; Note Off releases them.

## Compile & upload

```bash
arduino-cli compile --fqbn teensy:avr:teensy40 \
  --build-property "build.usbtype=USB_MIDI_SERIAL" .

arduino-cli upload --fqbn teensy:avr:teensy40 --port /dev/ttyACM0 .
```

## Required libraries

```bash
arduino-cli lib install "MIDI Library"
arduino-cli lib install "Encoder"
arduino-cli lib install "LiquidCrystal I2C"
```

(Teensy Audio library is bundled with the Teensy core.)

## Notes / things to tune

- VCO1 is currently treated as the "base" pitch reference; VCO2 detune is
  applied relative to it on each Note On.
- LFO is wired into the FM input of both VCOs — at depth 0 it has no effect.
  Increase "LFO Depth" to hear vibrato.
- "Filter Env Amt" controls `octaveControl()` on the state-variable filter —
  this scales how many octaves ADSR1 can sweep the cutoff.
- If the filter self-oscillates unpleasantly at high resonance, that's normal
  SVF behaviour near Q=5.
