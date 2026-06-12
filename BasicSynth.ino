// =============================================================================
// BasicSynth.ino  —  Teensy 4.0 + Audio Shield
// =============================================================================
// Architecture:
//   VCO1 + VCO2 --> mixer --> VCF --> VCA --> output
//   ADSR1 -> modulates VCF cutoff
//   ADSR2 -> modulates VCA (amplitude envelope)
//   LFO   -> modulates VCO pitch (vibrato) -- can be repurposed
//
// UI:
//   Encoder 1 (param select) : scroll through parameter list
//   Encoder 2 (value edit)   : change value of selected parameter
//   20x4 I2C LCD shows parameter list, with ">" cursor on selected row
//
// Wiring (no pin conflicts with Audio Shield):
//   Encoder 1 (param select): A=2,  B=3,  Btn=4
//   Encoder 2 (value edit)  : A=14, B=15, Btn=16
//   Display: I2C SDA=18, SCL=19 (shared bus with Audio Shield), address 0x27
//   Audio Shield: GND, 3.3V, SDA=18, SCL=19, MOSI=11, MISO=12, SCK=13,
//                 MEMCS=6, MCLK=23, BCLK=21, RX=8, TX=7, LRCLK=20
//
// MIDI: USB MIDI note on/off triggers both envelopes
// =============================================================================

#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <Encoder.h>
#include <LiquidCrystal_I2C.h>
#include <MIDI.h>

// =============================================================================
// AUDIO OBJECTS
// =============================================================================
AudioSynthWaveform       vco1;
AudioSynthWaveform       vco2;
AudioSynthWaveform       lfo;
AudioMixer4              vcoMix;
AudioFilterStateVariable vcf;
AudioEffectEnvelope      adsrFilter;   // ADSR1 -> filter cutoff mod
AudioEffectEnvelope      adsrAmp;      // ADSR2 -> amplitude (VCA)
AudioAmplifier           vca;
AudioOutputI2S           audioOut;
AudioControlSGTL5000     audioShield;

// ── Patch cords ───────────────────────────────────────────────────────────────
// VCO1 + VCO2 -> mixer
AudioConnection patchCord1(vco1, 0, vcoMix, 0);
AudioConnection patchCord2(vco2, 0, vcoMix, 1);

// LFO -> frequency modulation input of VCO1 and VCO2 (vibrato)
AudioConnection patchCord3(lfo, 0, vco1, 0);   // FM input
AudioConnection patchCord4(lfo, 0, vco2, 0);   // FM input

// mixer -> filter input
AudioConnection patchCord5(vcoMix, 0, vcf, 0);

// ADSR1 -> filter envelope mod input (CV input on the SVF)
AudioConnection patchCord6(adsrFilter, 0, vcf, 1);

// filter LP output -> VCA input
AudioConnection patchCord7(vcf, 0, vca, 0);

// ADSR2 -> VCA gain modulation (multiply input)
AudioConnection patchCord8(adsrAmp, 0, vca, 1);

// VCA -> output L/R
AudioConnection patchCord9(vca, 0, audioOut, 0);
AudioConnection patchCord10(vca, 0, audioOut, 1);

// =============================================================================
// MIDI
// =============================================================================
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

// =============================================================================
// ENCODERS
// =============================================================================
// Encoder 1 - parameter select
Encoder encSelect(2, 3);
const int BTN_SELECT = 4;

// Encoder 2 - value edit
Encoder encValue(14, 15);
const int BTN_VALUE = 16;

long lastSelectPos = 0;
long lastValuePos  = 0;

// =============================================================================
// DISPLAY
// =============================================================================
LiquidCrystal_I2C lcd(0x27, 20, 4);

// =============================================================================
// PARAMETER SYSTEM
// =============================================================================
// Each parameter has: name, min, max, step, current value, and an apply function

enum ParamID {
  P_VCO1_WAVE,
  P_VCO2_WAVE,
  P_VCO2_DETUNE,
  P_LFO_RATE,
  P_LFO_DEPTH,
  P_VCF_CUTOFF,
  P_VCF_RESONANCE,
  P_ADSR1_ATTACK,
  P_ADSR1_DECAY,
  P_ADSR1_SUSTAIN,
  P_ADSR1_RELEASE,
  P_ADSR2_ATTACK,
  P_ADSR2_DECAY,
  P_ADSR2_SUSTAIN,
  P_ADSR2_RELEASE,
  P_FILTER_ENV_AMT,
  NUM_PARAMS
};

struct Param {
  const char* name;
  float minVal;
  float maxVal;
  float step;
  float value;
  const char* unit;
};

Param params[NUM_PARAMS] = {
  // name              min     max     step   value  unit
  { "VCO1 Wave",       0,      4,      1,     0,     ""    },  // 0=sine,1=saw,2=square,3=tri,4=pulse
  { "VCO2 Wave",       0,      4,      1,     1,     ""    },
  { "VCO2 Detune",     -100,   100,    1,     0,     "ct"  },
  { "LFO Rate",        0.1,    20.0,   0.1,   5.0,   "Hz"  },
  { "LFO Depth",       0,      100,    1,     0,     "%"   },
  { "VCF Cutoff",      20,     20000,  50,    2000,  "Hz"  },
  { "VCF Resonance",   0.7,    5.0,    0.1,   0.7,   "Q"   },
  { "ADSR1 Attack",    1,      2000,   10,    10,    "ms"  },
  { "ADSR1 Decay",     1,      2000,   10,    200,   "ms"  },
  { "ADSR1 Sustain",   0,      100,    5,     50,    "%"   },
  { "ADSR1 Release",   1,      5000,   10,    200,   "ms"  },
  { "ADSR2 Attack",    1,      2000,   10,    5,     "ms"  },
  { "ADSR2 Decay",     1,      2000,   10,    150,   "ms"  },
  { "ADSR2 Sustain",   0,      100,    5,     70,    "%"   },
  { "ADSR2 Release",   1,      5000,   10,    300,   "ms"  },
  { "Filter Env Amt",  0,      100,    5,     50,    "%"   },
};

int selectedParam = 0;
bool displayDirty = true;
float currentBaseFreq = 220.0f; // tracks VCO1 base frequency for detune calc

// Waveform name lookup for VCO wave param
const char* waveNames[] = { "Sine", "Saw", "Square", "Tri", "Pulse" };
int waveShapes[] = { WAVEFORM_SINE, WAVEFORM_SAWTOOTH, WAVEFORM_SQUARE,
                     WAVEFORM_TRIANGLE, WAVEFORM_PULSE };

// =============================================================================
// APPLY PARAMETER -> AUDIO ENGINE
// =============================================================================
void applyParam(int id) {
  float v = params[id].value;

  switch (id) {
    case P_VCO1_WAVE:
      vco1.begin(0.4f, currentBaseFreq, waveShapes[(int)v]);
      break;

    case P_VCO2_WAVE: {
      float detuneCents = params[P_VCO2_DETUNE].value;
      float vco2freq = currentBaseFreq * powf(2.0f, detuneCents / 1200.0f);
      vco2.begin(0.4f, vco2freq, waveShapes[(int)v]);
      break;
    }

    case P_VCO2_DETUNE: {
      float detuned = currentBaseFreq * powf(2.0f, v / 1200.0f);
      vco2.frequency(detuned);
      break;
    }

    case P_LFO_RATE:
      lfo.frequency(v);
      break;

    case P_LFO_DEPTH:
      // LFO depth controls its amplitude (0-1), used as FM index
      lfo.amplitude(v / 100.0f);
      break;

    case P_VCF_CUTOFF:
      vcf.frequency(v);
      break;

    case P_VCF_RESONANCE:
      vcf.resonance(v);
      break;

    case P_ADSR1_ATTACK:
      adsrFilter.attack(v);
      break;
    case P_ADSR1_DECAY:
      adsrFilter.decay(v);
      break;
    case P_ADSR1_SUSTAIN:
      adsrFilter.sustain(v / 100.0f);
      break;
    case P_ADSR1_RELEASE:
      adsrFilter.release(v);
      break;

    case P_ADSR2_ATTACK:
      adsrAmp.attack(v);
      break;
    case P_ADSR2_DECAY:
      adsrAmp.decay(v);
      break;
    case P_ADSR2_SUSTAIN:
      adsrAmp.sustain(v / 100.0f);
      break;
    case P_ADSR2_RELEASE:
      adsrAmp.release(v);
      break;

    case P_FILTER_ENV_AMT:
      // octaveControl scales how much the SVF CV input affects cutoff (in octaves)
      vcf.octaveControl(v / 100.0f * 7.0f); // 0-100% -> 0-7 octaves
      break;
  }
}

void applyAllParams() {
  for (int i = 0; i < NUM_PARAMS; i++) applyParam(i);
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(115200);

  AudioMemory(40);

  audioShield.enable();
  audioShield.volume(0.6f);

  // ── VCOs ──────────────────────────────────────────────────────────────────
  vco1.begin(0.4f, 220.0f, WAVEFORM_SINE);
  vco2.begin(0.4f, 220.0f, WAVEFORM_SAWTOOTH);

  // ── Mixer levels ──────────────────────────────────────────────────────────
  vcoMix.gain(0, 0.5f);  // VCO1
  vcoMix.gain(1, 0.5f);  // VCO2
  vcoMix.gain(2, 0.0f);
  vcoMix.gain(3, 0.0f);

  // ── LFO ───────────────────────────────────────────────────────────────────
  lfo.begin(0.0f, 5.0f, WAVEFORM_SINE); // depth starts at 0

  // ── Filter ────────────────────────────────────────────────────────────────
  vcf.frequency(2000);
  vcf.resonance(0.7);
  vcf.octaveControl(3.5f);

  // ── Envelopes ─────────────────────────────────────────────────────────────
  adsrFilter.attack(10);
  adsrFilter.decay(200);
  adsrFilter.sustain(0.5f);
  adsrFilter.release(200);

  adsrAmp.attack(5);
  adsrAmp.decay(150);
  adsrAmp.sustain(0.7f);
  adsrAmp.release(300);

  // ── VCA ───────────────────────────────────────────────────────────────────
  vca.gain(1.0f);

  // Apply all stored param values so engine matches the param table
  applyAllParams();

  // ── Encoders ──────────────────────────────────────────────────────────────
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_VALUE, INPUT_PULLUP);

  // ── Display ───────────────────────────────────────────────────────────────
  lcd.init();
  lcd.backlight();
  lcd.clear();

  // ── MIDI ──────────────────────────────────────────────────────────────────
  MIDI.begin(MIDI_CHANNEL_OMNI);
  MIDI.setHandleNoteOn(onNoteOn);
  MIDI.setHandleNoteOff(onNoteOff);

  Serial.println("BasicSynth ready");
}

// =============================================================================
// MIDI HANDLERS
// =============================================================================
int currentNote = -1;

void onNoteOn(byte channel, byte note, byte velocity) {
  if (velocity == 0) { onNoteOff(channel, note, velocity); return; }

  currentNote = note;
  float freq = 440.0f * powf(2.0f, (note - 69) / 12.0f);
  currentBaseFreq = freq;

  vco1.frequency(freq);

  // VCO2 with detune applied
  float detuneCents = params[P_VCO2_DETUNE].value;
  vco2.frequency(freq * powf(2.0f, detuneCents / 1200.0f));

  adsrFilter.noteOn();
  adsrAmp.noteOn();
}

void onNoteOff(byte channel, byte note, byte velocity) {
  if (note == currentNote) {
    adsrFilter.noteOff();
    adsrAmp.noteOff();
    currentNote = -1;
  }
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
  MIDI.read();
  handleEncoders();
  if (displayDirty) {
    updateDisplay();
    displayDirty = false;
  }
}

// =============================================================================
// ENCODER HANDLING
// =============================================================================
void handleEncoders() {
  // ── Encoder 1: parameter select ──────────────────────────────────────────
  long selPos = encSelect.read() / 4;
  if (selPos != lastSelectPos) {
    int delta = (int)(selPos - lastSelectPos);
    selectedParam = constrain(selectedParam + delta, 0, NUM_PARAMS - 1);
    lastSelectPos = selPos;
    displayDirty = true;
  }

  // ── Encoder 2: value edit ────────────────────────────────────────────────
  long valPos = encValue.read() / 4;
  if (valPos != lastValuePos) {
    int delta = (int)(valPos - lastValuePos);
    Param& p = params[selectedParam];
    p.value = constrain(p.value + delta * p.step, p.minVal, p.maxVal);
    lastValuePos = valPos;
    applyParam(selectedParam);
    displayDirty = true;
  }
}

// =============================================================================
// DISPLAY
// =============================================================================
// Shows 4 parameters at a time, scrolling window follows selectedParam.
// Row 0: shows the topmost visible parameter, etc.
// ">" marks the selected row.

String formatValue(const Param& p) {
  if (&p == &params[P_VCO1_WAVE] || &p == &params[P_VCO2_WAVE]) {
    return String(waveNames[(int)p.value]);
  }
  if (p.step >= 1.0f) {
    return String((int)p.value) + p.unit;
  } else {
    return String(p.value, 1) + p.unit;
  }
}

void printParamRow(int row, int paramIdx) {
  Param& p = params[paramIdx];
  String cursor = (paramIdx == selectedParam) ? ">" : " ";
  String name = String(p.name);
  String val = formatValue(p);

  // Build line: "> Name        Value"
  String line = cursor + name;
  int padding = 20 - line.length() - val.length();
  for (int i = 0; i < padding; i++) line += ' ';
  line += val;
  line = line.substring(0, 20);

  lcd.setCursor(0, row);
  lcd.print(line);
}

void updateDisplay() {
  // Scrolling window: keep selectedParam visible within 4 rows
  static int windowStart = 0;
  if (selectedParam < windowStart) windowStart = selectedParam;
  if (selectedParam > windowStart + 3) windowStart = selectedParam - 3;
  if (windowStart > NUM_PARAMS - 4) windowStart = NUM_PARAMS - 4;
  if (windowStart < 0) windowStart = 0;

  for (int row = 0; row < 4; row++) {
    int idx = windowStart + row;
    if (idx < NUM_PARAMS) {
      printParamRow(row, idx);
    } else {
      lcd.setCursor(0, row);
      lcd.print("                    "); // blank line
    }
  }
}
