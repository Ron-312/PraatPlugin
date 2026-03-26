# fuzz_distortion.praat
#
# Applies soft-clipping saturation to the audio using a tanh transfer curve —
# the same mathematical shape as a tube amplifier or tape saturation.
#
# Low drive (~2): gentle warmth, adds even harmonics
# Mid drive (~5): clearly distorted, guitar-fuzz territory
# High drive (~10): heavily clipped, almost square-wave fuzz
#
# Output: processed audio written to outputFile
# Called by PraatPlugin as:
#   Praat --run fuzz_distortion.praat /path/input.wav /path/output.wav

form Fuzz Distortion
    sentence inputFile /tmp/input.wav
    sentence outputFile /tmp/output.wav
endform

# Default drive amount.  Edit this file to change the saturation character:
#   2 — gentle warmth   5 — guitar fuzz   10 — heavy square-wave clip
drive = 5.0

# Load the input
sound = Read from file: inputFile$

# Normalise to 0 dBFS first so the drive amount is consistent regardless of
# how loud the original recording was
selectObject: sound
Scale peak: 0.99

# Build the saturation formula as a string with literal values.
# tanh(x * drive) / tanh(drive) keeps the output in [-1, 1] for any drive.
# Embedding the literals avoids any potential scoping issue in Praat's
# Formula interpreter when the script is called via --run.
norm$ = string$(tanh(drive))
formula$ = "tanh(self * " + string$(drive) + ") / " + norm$

selectObject: sound
Formula: formula$

# Pull back slightly below full scale to be DAW-friendly
selectObject: sound
Scale peak: 0.95

# Write processed audio
selectObject: sound
Save as WAV file: outputFile$

# Text output for the results panel
writeInfoLine:  "fuzz_distortion: done"
appendInfoLine: "drive: " + string$(drive) + "x"
appendInfoLine: "character: soft saturation (tanh curve)"
appendInfoLine: "note: reload the waveform to see the processed audio"

removeObject: sound
