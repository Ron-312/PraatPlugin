# robot_voice.praat
#
# Flattens the pitch to a single constant frequency, making any voice sound
# robotic and monotone.  Works by replacing the PitchTier inside a Praat
# Manipulation object and resynthesising using PSOLA (overlap-add).
#
# target_pitch 80  — deep robot (think HAL 9000)
# target_pitch 120 — neutral robot
# target_pitch 200 — high robot / answering machine
#
# Output: processed audio written to outputFile
# Called by PraatPlugin as:
#   Praat --run robot_voice.praat /path/input.wav /path/output.wav

form Robot Voice
    sentence inputFile /tmp/input.wav
    sentence outputFile /tmp/output.wav
endform

# Default target pitch.  Edit this file to change the robot character:
#   80  — deep (HAL 9000)   120 — neutral   200 — high / answering machine
target_pitch = 120

# Load sound and measure duration
sound = Read from file: inputFile$
selectObject: sound
dur = Get total duration

# Build a Manipulation object (contains a copy of the audio plus pitch/duration
# tiers extracted by Praat's autocorrelation pitch tracker)
selectObject: sound
manip = To Manipulation: 0.01, 75, 600

# Pull out the pitch tier so we can overwrite it
selectObject: manip
pt = Extract pitch tier

# Erase all natural pitch variation and replace with a single flat pitch
selectObject: pt
Remove points between: 0, dur
Add point: 0.01, target_pitch
Add point: dur - 0.01, target_pitch

# Put the modified pitch tier back into the manipulation object
selectObject: manip
plusObject: pt
Replace pitch tier

# Resynthesize using PSOLA — preserves timing and spectral shape, only
# changes pitch trajectory
selectObject: manip
output = Get resynthesis (overlap-add)

selectObject: output
Save as WAV file: outputFile$

# Text output for the results panel
writeInfoLine:  "robot_voice: done"
appendInfoLine: "flat_pitch_hz: " + string$(target_pitch)
appendInfoLine: "duration_s: "    + fixed$(dur, 3)
appendInfoLine: "note: reload the waveform to hear the effect"

removeObject: sound, manip, pt, output
