# spectral_reverb.praat
#
# Stochastic convolution reverb: synthesises an impulse response (IR)
# from a decaying Gaussian noise signal, then convolves it with the dry
# audio.  A bandpass filter (100–4000 Hz) shapes the wet return.
#
# The IR is generated in one vectorised formula call (fast).  No per-sample
# Praat loops are used — the whole script typically completes in 1–3 seconds.
#
# Parameters (controlled from the plugin UI):
#   tail_s       1.2  — reverb tail length in seconds (0.3 = tight, 3.0 = ambient)
#   wet_mix      0.5  — 0 = fully dry, 1 = fully wet
#   decay_speed  4.0  — IR envelope decay rate (2 = slow/lush, 8 = short/tight)
#
# Output: processed audio written to outputFile
# Called by PraatPlugin as:
#   Praat --run spectral_reverb.praat /path/input.wav /path/output.wav [params...]

form Spectral Reverb
    sentence inputFile /tmp/input.wav
    sentence outputFile /tmp/output.wav
    positive tail_s 1.2
    real wet_mix 0.5
    positive decay_speed 4.0
endform

dry_mix = 1 - wet_mix

# Bandpass limits — fixed; shape the reverb colour
bp_low_hz  = 100
bp_high_hz = 4000

# Load
sound = Read from file: inputFile$
selectObject: sound
dur = Get total duration
sr  = Get sampling frequency
nch = Get number of channels

# Build a dry copy with a silence tail appended so the convolution tail
# has room to decay.  Use Concatenate rather than Lengthen (overlap-add)
# to avoid a pitch-analysis dependency on the content.
selectObject: sound
Copy: "reverb_dry"
dry_copy = selected("Sound")

silence = Create Sound from formula: "sil", nch, 0, tail_s, sr, "0"

selectObject: dry_copy
plusObject: silence
Concatenate
dry_padded = selected("Sound")
removeObject: dry_copy, silence

selectObject: dry_padded
dur_padded = Get total duration

# ---------------------------------------------------------------
# Synthesise impulse response in one vectorised formula call.
# randomGauss(0,1) gives independent Gaussian noise per sample;
# multiplying by exp(-decay_speed * x) gives exponential decay.
# This runs entirely in compiled C — milliseconds, not seconds.
# ---------------------------------------------------------------
decay_str$ = string$(decay_speed)
ir = Create Sound from formula: "ir", 1, 0, tail_s, sr, "randomGauss(0,1) * exp(-" + decay_str$ + " * x)"

# Shape the IR with a bandpass so only the mid-range frequencies
# contribute to the wet return (removes low-frequency mud and high hiss).
selectObject: ir
Filter (pass Hann band): bp_low_hz, bp_high_hz, 100
ir_filtered = selected("Sound")
removeObject: ir

# Normalise IR
selectObject: ir_filtered
Scale peak: 0.99

# ---------------------------------------------------------------
# Convolution
# ---------------------------------------------------------------
selectObject: dry_padded
plusObject: ir_filtered
Convolve: "sum", "zero"
wet_conv = selected("Sound")

# Trim convolution output to the padded duration
selectObject: wet_conv
Extract part: 0, dur_padded, "rectangular", 1, "no"
wet_trimmed = selected("Sound")
removeObject: wet_conv

# Bandpass the wet signal to match the IR's spectral shape
selectObject: wet_trimmed
Filter (pass Hann band): bp_low_hz, bp_high_hz, 100
wet_filtered = selected("Sound")
removeObject: wet_trimmed

# Normalise wet before mixing
selectObject: wet_filtered
Scale peak: 0.99

# ---------------------------------------------------------------
# Dry/wet mix — write into dry_padded in-place
# ---------------------------------------------------------------
wet_id$ = string$(wet_filtered)
dry_str$ = string$(dry_mix)
wet_str$ = string$(wet_mix)

selectObject: dry_padded
Formula: dry_str$ + " * self + " + wet_str$ + " * object[" + wet_id$ + ", 1, col]"

# Trim to original duration
selectObject: dry_padded
Extract part: 0, dur, "rectangular", 1, "no"
result = selected("Sound")
removeObject: dry_padded

# Cosine fade-out over the last 5% to avoid a hard cutoff
fade_start = dur * 0.95
fade_str$  = string$(fade_start)
dur_str$   = string$(dur)
selectObject: result
Formula: "if x > " + fade_str$ + " then self * (0.5 + 0.5 * cos(pi * (x - " + fade_str$ + ") / (" + dur_str$ + " - " + fade_str$ + "))) else self fi"

# Final peak normalisation
selectObject: result
Scale peak: 0.95

# Write output
selectObject: result
Save as WAV file: outputFile$

# Results for the plugin results panel
writeInfoLine:  "effect: spectral_reverb"
appendInfoLine: "tail_s: "       + fixed$(tail_s, 2)
appendInfoLine: "wet_mix: "      + fixed$(wet_mix, 2)
appendInfoLine: "dry_mix: "      + fixed$(dry_mix, 2)
appendInfoLine: "decay_speed: "  + fixed$(decay_speed, 1)
appendInfoLine: "bp_low_hz: "    + string$(bp_low_hz)
appendInfoLine: "bp_high_hz: "   + string$(bp_high_hz)
appendInfoLine: "duration_s: "   + fixed$(dur, 3)
appendInfoLine: "note: reload the waveform to hear the reverb"

removeObject: sound, ir_filtered, wet_filtered, result
