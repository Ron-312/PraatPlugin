# formant_analysis.praat
#
# Measures the first four formant frequencies (F1-F4) at the midpoint
# of the provided sound file. Common in vowel analysis.
#
# Called by PraatPlugin as:
#   Praat --run formant_analysis.praat /path/to/input.wav
#
# Output: KEY: value lines read by ResultParser (see docs/adr/ADR-006)

form Formant Analysis
    sentence inputFile /tmp/input.wav
    sentence outputFile /tmp/output.wav
endform
# outputFile is accepted but not used — this script produces text results only.

# Settings — sensible defaults for adult speech
maxFormant    = 5500
windowLength  = 0.025

# Load the audio file
Read from file: inputFile$
duration = Get total duration

# Extract formants using Burg algorithm
To Formant (burg): 0, 5, maxFormant, windowLength, 50

# Measure at the midpoint of the file
midpoint = duration / 2

f1 = Get value at time: 1, midpoint, "Hertz", "Linear"
f2 = Get value at time: 2, midpoint, "Hertz", "Linear"
f3 = Get value at time: 3, midpoint, "Hertz", "Linear"
f4 = Get value at time: 4, midpoint, "Hertz", "Linear"

writeInfoLine:  "f1: "       + fixed$(f1, 0) + " Hz"
appendInfoLine: "f2: "       + fixed$(f2, 0) + " Hz"
appendInfoLine: "f3: "       + fixed$(f3, 0) + " Hz"
appendInfoLine: "f4: "       + fixed$(f4, 0) + " Hz"
appendInfoLine: "midpoint: " + fixed$(midpoint, 3) + " s"
appendInfoLine: "duration: " + fixed$(duration, 3) + " s"
