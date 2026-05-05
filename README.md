# TeensySynth
semi modular (in code) synthesizer

This tempalte uses a module-based architecture — each "library" (Encoder, Display, VCO, Filter, etc.) lives in its own .h file with a clean interface, and your main .ino file just includes what it needs and calls begin() / update() in the right places. This makes it trivial to build a new synth by including a different combination of modules.
