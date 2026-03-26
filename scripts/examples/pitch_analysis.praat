# pitch_analysis.praat
#
# Analyses the pitch (fundamental frequency) of a sound file.
#
# Called by PraatPlugin as:
#   Praat --run pitch_analysis.praat /path/to/input.wav
#
# Output: KEY: value lines read by ResultParser (see docs/adr/ADR-006)

form Pitch Analysis
    sentence inputFile /tmp/input.wav
    sentence outputFile /tmp/output.wav
endform
# outputFile is accepted but not used — this script produces text results only.

# Settings — covers typical speech range
minPitch = 75
maxPitch = 600

# Load the audio file
Read from file: inputFile$

# Extract pitch using autocorrelation
To Pitch: 0, minPitch, maxPitch

# Compute statistics over the whole file (0 to 0 = full duration)
mean    = Get mean:               0, 0, "Hertz"
minimum = Get minimum:            0, 0, "Hertz", "Parabolic"
maximum = Get maximum:            0, 0, "Hertz", "Parabolic"
stdev   = Get standard deviation: 0, 0, "Hertz"

writeInfoLine:  "mean_pitch: "  + fixed$(mean,    1) + " Hz"
appendInfoLine: "min_pitch: "   + fixed$(minimum, 1) + " Hz"
appendInfoLine: "max_pitch: "   + fixed$(maximum, 1) + " Hz"
appendInfoLine: "pitch_stdev: " + fixed$(stdev,   1) + " Hz"
