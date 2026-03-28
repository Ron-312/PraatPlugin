# wavefolder.praat
#
# Wavefolding distortion: the signal folds back on itself when it
# exceeds a threshold, generating rich harmonic content.  This is
# the classic Buchla/Serge synthesizer saturation character.
#
# Parameters (controlled from the plugin UI):
#   threshold       0.3   — fold starts here (0.1 = aggressive, 0.8 = subtle)
#   input_gain_dB   12.0  — drives signal into fold territory
#   fold_depth      1.0   — mirror reflection amount past threshold
#   fold_iterations 1     — how many times to fold (1 = clean, 3 = dense)
#   output_gain_dB  -6.0  — compensates for loudness increase
#
# Output: processed audio written to outputFile
# Called by PraatPlugin as:
#   Praat --run wavefolder.praat /path/input.wav /path/output.wav [params...]

form Wavefolder
    sentence inputFile /tmp/input.wav
    sentence outputFile /tmp/output.wav
    positive threshold 0.3
    real input_gain_dB 12.0
    positive fold_depth 1.0
    integer fold_iterations 1
    real output_gain_dB -6.0
endform

# Derived linear gain values
input_gain_linear  = 10 ^ (input_gain_dB  / 20)
output_gain_linear = 10 ^ (output_gain_dB / 20)

# Load
sound = Read from file: inputFile$
selectObject: sound
dur = Get total duration

# Normalise input to consistent level before driving
selectObject: sound
Scale peak: 0.99

# Build formula strings with literal values embedded.
# Praat's --run interpreter has no access to outer variable scope inside
# Formula:, so we construct each formula as a string with all values baked in.
inGain$   = string$(input_gain_linear)
outGain$  = string$(output_gain_linear)
thresh$   = string$(threshold)
depth$    = string$(fold_depth)

# Input gain — drive the signal hard
selectObject: sound
Formula: "self * " + inGain$

# Foldback: fold positive and negative sides independently
for iteration from 1 to fold_iterations
    selectObject: sound
    # Positive side: reflect anything above threshold back downward
    Formula: "if self > " + thresh$ + " then " + thresh$ + " - (self - " + thresh$ + ") * " + depth$ + " else self fi"
    # Negative side: reflect anything below -threshold back upward
    Formula: "if self < -" + thresh$ + " then -" + thresh$ + " + (abs(self) - " + thresh$ + ") * " + depth$ + " else self fi"
endfor

# Output gain
selectObject: sound
Formula: "self * " + outGain$

# DC offset removal (foldback can introduce mild DC)
selectObject: sound
Subtract mean

# Peak normalization guard — clamp to 0.95 if the fold pushed us over
selectObject: sound
peak_pos = Get maximum: 0, 0, "Sinc70"
peak_neg = Get minimum: 0, 0, "Sinc70"
peak_abs = max(abs(peak_pos), abs(peak_neg))

if peak_abs > 0.95
    peak_str$ = string$(peak_abs)
    Formula: "self * 0.95 / " + peak_str$
endif

# Write output
selectObject: sound
Save as WAV file: outputFile$

# Results for the plugin results panel
writeInfoLine:  "effect: wavefolder"
appendInfoLine: "threshold: "        + fixed$(threshold, 3)
appendInfoLine: "input_gain_dB: "    + fixed$(input_gain_dB, 1)
appendInfoLine: "fold_depth: "       + fixed$(fold_depth, 2)
appendInfoLine: "fold_iterations: "  + string$(fold_iterations)
appendInfoLine: "output_gain_dB: "   + fixed$(output_gain_dB, 1)
appendInfoLine: "peak_before_norm: " + fixed$(peak_abs, 3)
appendInfoLine: "duration_s: "       + fixed$(dur, 3)
appendInfoLine: "note: reload the waveform to hear the folded audio"

removeObject: sound
