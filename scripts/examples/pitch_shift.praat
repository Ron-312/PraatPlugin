# pitch_shift.praat
#
# Transposes the pitch by a given number of semitones while keeping the
# timing and vocal-tract character (formants) unchanged.
# Uses PSOLA resynthesis via a Praat Manipulation object.
#
# semitones  +12 — one octave up (chipmunk / helium effect)
# semitones  +7  — fifth up (bright, playful)
# semitones  -7  — fifth down (deeper, authoritative)
# semitones  -12 — one octave down (very deep)
#
# Note: extreme shifts (>12 semitones) will produce audible artefacts —
# that's part of the character.
#
# Output: processed audio written to outputFile
# Called by PraatPlugin as:
#   Praat --run pitch_shift.praat /path/input.wav /path/output.wav

form Pitch Shift
    sentence inputFile /tmp/input.wav
    sentence outputFile /tmp/output.wav
endform

# Default pitch shift in semitones.  Edit this file to change the amount:
#   +12 — one octave up   +7 — fifth up   -7 — fifth down   -12 — octave down
semitones = 7.0

# Load sound and measure duration
sound = Read from file: inputFile$
selectObject: sound
dur = Get total duration

# Build the Manipulation object
selectObject: sound
manip = To Manipulation: 0.01, 60, 700

# Extract the pitch tier and scale every frequency point by the semitone ratio.
# Formula on a PitchTier: 'self' is the Hz value at each point.
factor = 2 ^ (semitones / 12)

selectObject: manip
pt = Extract pitch tier

selectObject: pt
Formula: "self * factor"

# Put the shifted pitch tier back
selectObject: manip
plusObject: pt
Replace pitch tier

# Resynthesize
selectObject: manip
output = Get resynthesis (overlap-add)

selectObject: output
Save as WAV file: outputFile$

# Text output for the results panel
writeInfoLine:  "pitch_shift: done"
appendInfoLine: "semitones: "  + string$(semitones)
appendInfoLine: "ratio: "      + fixed$(factor, 3) + "x"
appendInfoLine: "duration_s: " + fixed$(dur, 3)
appendInfoLine: "note: reload the waveform to hear the shifted audio"

removeObject: sound, manip, pt, output
