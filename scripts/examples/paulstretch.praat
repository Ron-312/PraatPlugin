# paulstretch.praat
#
# Paulstretch: extreme time-stretching via FFT phase randomisation and
# overlap-add resynthesis.  Harmonic content is preserved; only the temporal
# evolution is smeared into a slow, shimmering cloud — the classic ambient-
# drone / infinite reverb effect.
#
# PERFORMANCE NOTE: This script uses a per-frame Praat loop with per-bin
# phase randomisation.  To complete within the plugin's 30-second timeout:
#   - Input is capped at 1 second (trimmed automatically if longer)
#   - Audio is resampled to 8000 Hz before processing
#   - window_s must stay ≤ 0.1 s
# At these settings a typical run takes 8–15 seconds.
# Use short clips (speech, a note, a drum hit) for best results.
#
# Parameters (controlled from the plugin UI):
#   stretch_factor  8.0  — output duration = stretch_factor × input duration
#   window_s        0.05 — FFT window length in seconds (≤ 0.1 for performance)
#
# Output: processed audio written to outputFile
# Called by PraatPlugin as:
#   Praat --run paulstretch.praat /path/input.wav /path/output.wav [params...]

form Paulstretch
    sentence inputFile /tmp/input.wav
    sentence outputFile /tmp/output.wav
    positive stretch_factor 8.0
    positive window_s 0.05
endform

# Processing sample rate — 8000 Hz keeps bin count low for performance
target_sr = 8000

# Maximum input duration — DO NOT increase; higher values will time out
max_input_s = 1.0

# Load and convert to mono
sound_raw = Read from file: inputFile$
selectObject: sound_raw
nch = Get number of channels

if nch > 1
    selectObject: sound_raw
    Convert to mono
    sound_mono = selected("Sound")
    removeObject: sound_raw
else
    sound_mono = sound_raw
endif

# Resample to target_sr for performance
selectObject: sound_mono
Resample: target_sr, 50
sound_rs = selected("Sound")
removeObject: sound_mono

# Record original duration (before any trimming) for the info output
selectObject: sound_rs
dur_original = Get total duration

# Trim to max_input_s
if dur_original > max_input_s
    selectObject: sound_rs
    Extract part: 0, max_input_s, "Hanning", 1, "no"
    sound_src = selected("Sound")
    removeObject: sound_rs
    dur_in = max_input_s
else
    sound_src = sound_rs
    dur_in = dur_original
endif

# Normalise
selectObject: sound_src
Scale peak: 0.99

sr = target_sr

# ---------------------------------------------------------------
# Frame dimensions
# ---------------------------------------------------------------
win_samples = round(window_s * sr)
hop_in      = round(win_samples / 4)         ; 75 % overlap between analysis frames
hop_out     = round(hop_in * stretch_factor) ; stretched spacing in output

n_in      = Get number of samples
n_frames  = floor((n_in - win_samples) / hop_in) + 1
dur_out   = n_frames * hop_out / sr

if dur_out < 0.1
    dur_out = 0.1
endif

# Create output accumulator (silence)
output = Create Sound from formula: "paulstretch", 1, 0, dur_out, sr, "0"

# Each frame's contribution is divided by this to normalise the overlap
norm_factor = win_samples / 4.0

# ---------------------------------------------------------------
# Main overlap-add loop
# ---------------------------------------------------------------
for frame from 0 to n_frames - 1
    in_start  = frame * hop_in  + 1
    out_start = frame * hop_out + 1

    # Extract one windowed analysis frame
    selectObject: sound_src
    t_start = (in_start - 1) / sr
    t_end   = (in_start - 1 + win_samples) / sr
    Extract part: t_start, t_end, "Hanning", 1, "no"
    frame_win = selected("Sound")

    # FFT
    selectObject: frame_win
    To Spectrum: "yes"
    spec = selected("Spectrum")

    # Randomise all bin phases while keeping magnitudes unchanged.
    # This is the core of Paulstretch: each resynthesised frame has the
    # same spectral content as the original but a random phase relationship,
    # which smears temporal detail into sustained spectral colour.
    selectObject: spec
    n_bins = Get number of bins
    for b from 1 to n_bins
        re  = Get real value in bin: b
        im  = Get imaginary value in bin: b
        mag = sqrt(re * re + im * im)
        rnd = randomUniform(0, 2 * pi)
        Set real value in bin:      b, mag * cos(rnd)
        Set imaginary value in bin: b, mag * sin(rnd)
    endfor

    # IFFT back to time domain
    selectObject: spec
    To Sound
    frame_out = selected("Sound")

    # Overlap-add: accumulate frame into output at the stretched position
    selectObject: frame_out
    frame_n = Get number of samples

    for s from 1 to frame_n
        out_idx = out_start + s - 1
        if out_idx >= 1 and out_idx <= round(dur_out * sr)
            val = Get value at sample number: 1, s
            selectObject: output
            cur = Get value at sample number: 1, out_idx
            Set value at sample number: 1, out_idx, cur + val / norm_factor
        endif
    endfor

    removeObject: frame_win, spec, frame_out
endfor

# Final normalisation
selectObject: output
Scale peak: 0.95

# Write output
selectObject: output
Save as WAV file: outputFile$

# Results for the plugin results panel
writeInfoLine:  "effect: paulstretch"
appendInfoLine: "stretch_factor: "  + fixed$(stretch_factor, 1)
appendInfoLine: "window_s: "        + fixed$(window_s, 3)
appendInfoLine: "sample_rate: "     + string$(sr)
appendInfoLine: "duration_in_s: "   + fixed$(dur_in, 3)
appendInfoLine: "duration_out_s: "  + fixed$(dur_out, 3)
appendInfoLine: "n_frames: "        + string$(n_frames)
appendInfoLine: "note: input capped at 1s / 8kHz for performance"
appendInfoLine: "note: reload the waveform to hear the stretch"

removeObject: sound_src, output
