# stereo_phaser.praat
#
# Stereo phaser: a time-varying comb filter effect.  An LFO sweeps a
# short delay, creating moving spectral notches (the classic jet-plane
# swoosh).  Left and right channels use opposite LFO phases for a wide,
# swirling stereo image.
#
# Parameters (controlled from the plugin UI):
#   rate_Hz          0.6  — LFO speed (0.1 = slow, 4.0 = fast wobble)
#   depth_ms         1.5  — modulation depth in milliseconds
#   center_delay_ms  2.5  — base delay around which LFO sweeps
#   feedback         0.4  — resonance amount (0 = subtle, 0.85 = metallic)
#   dry_wet          0.5  — 0 = fully dry, 1 = fully wet
#
# Output: stereo WAV written to outputFile
# Called by PraatPlugin as:
#   Praat --run stereo_phaser.praat /path/input.wav /path/output.wav [params...]

form Stereo Phaser
    sentence inputFile /tmp/input.wav
    sentence outputFile /tmp/output.wav
    positive rate_Hz 0.6
    positive depth_ms 1.5
    positive center_delay_ms 2.5
    real feedback 0.4
    real dry_wet 0.5
endform

# Stereo phase offset: left and right LFO differ by 90 degrees (pi/2 radians).
# This is hardcoded — it is the defining character of a stereo phaser.
stereo_phase_rad = pi / 2

# Load
sound_raw = Read from file: inputFile$
selectObject: sound_raw
dur = Get total duration
sr  = Get sampling frequency
nch = Get number of channels

# The plugin always sends mono audio; convert to stereo so we can produce a
# wide stereo image.  If the file is already stereo, copy it unchanged.
if nch = 1
    selectObject: sound_raw
    Convert to stereo
    sound_stereo = selected("Sound")
    removeObject: sound_raw
else
    selectObject: sound_raw
    Copy: "phaser_src"
    sound_stereo = selected("Sound")
    removeObject: sound_raw
endif

# Create the output sound as a copy of the stereo source
selectObject: sound_stereo
Copy: "phaser_result"
result = selected("Sound")

# Convert delay parameters to sample-domain integers
base_samp = round(center_delay_ms * sr / 1000)
mod_samp  = round(depth_ms        * sr / 1000)

# Build formula strings with all numeric literals embedded.
# row = channel (1 = left, 2 = right); the stereo phase offset means
# left LFO starts at phase 0, right LFO starts at phase stereo_phase_rad.
src_id$    = string$(sound_stereo)
rate_str$  = string$(rate_Hz)
base_str$  = string$(base_samp)
mod_str$   = string$(mod_samp)
phase_str$ = string$(stereo_phase_rad)
fb_str$    = string$(feedback)
dry_str$   = string$(1 - dry_wet)
wet_str$   = string$(dry_wet)

# Delay column lookup: time-varying, per-channel, clamped to valid range.
# (row-1) is 0 for left, 1 for right — gives 0° and 90° LFO phase respectively.
delay_col$ = "max(1, min(ncol, col - round(" + base_str$ + " + " + mod_str$ + " * sin(2*pi*" + rate_str$ + "*x + (row-1)*" + phase_str$ + "))))"

# Optional feedback: mix a delayed copy of the source back into the result
# before the main comb-filter pass.  Increases resonance.
if feedback > 0
    selectObject: result
    Formula: "self + " + fb_str$ + " * object[" + src_id$ + ", row, " + delay_col$ + "]"
endif

# Main comb-filter mix: blend dry (self) with wet (time-varying delayed copy)
selectObject: result
Formula: dry_str$ + " * object[" + src_id$ + ", row, col] + " + wet_str$ + " * object[" + src_id$ + ", row, " + delay_col$ + "]"

# Normalise output
selectObject: result
Scale peak: 0.95

# Write stereo output
selectObject: result
Save as WAV file: outputFile$

# Results for the plugin results panel
writeInfoLine:  "effect: stereo_phaser"
appendInfoLine: "rate_hz: "          + fixed$(rate_Hz, 2)
appendInfoLine: "depth_ms: "         + fixed$(depth_ms, 2)
appendInfoLine: "center_delay_ms: "  + fixed$(center_delay_ms, 2)
appendInfoLine: "feedback: "         + fixed$(feedback, 2)
appendInfoLine: "dry_wet: "          + fixed$(dry_wet, 2)
appendInfoLine: "output_channels: 2"
appendInfoLine: "duration_s: "       + fixed$(dur, 3)
appendInfoLine: "note: reload the waveform to hear the phaser"

removeObject: sound_stereo, result
