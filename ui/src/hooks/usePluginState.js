// ─── usePluginState ───────────────────────────────────────────────────────────
//
// The single source of truth for all plugin state in the UI.
//
// State flows in one direction:
//   C++ timer (20fps) → emitEventIfBrowserIsVisible("stateUpdate", ...) →
//   onPluginEvent listener here → setState → React re-render
//
// Actions flow the other direction:
//   User interaction → action creator → sendToPlugin → C++ withEventListener
//
// Components only ever call { state, actions } from this hook — they never
// touch the bridge directly.
//
// Mock state (bottom of file) keeps the UI renderable during npm run dev.
// ─────────────────────────────────────────────────────────────────────────────

import { useState, useEffect, useCallback } from 'react'
import { sendToPlugin, onPluginEvent } from '../bridge/juceBridge'

// ── State shape ───────────────────────────────────────────────────────────────
// Every key here must have a matching property emitted by pushStateToWebView()
// in PraatPluginEditor.cpp.
const INITIAL_STATE = {
  scriptFolders:    [],       // {name: string, scripts: string[]}[]  — folder-organized
  selectedScript:   '',       // string    — "FOLDER/scriptName" or ''

  hasAudio:         false,    // bool      — an audio file is loaded
  hasProcessedAudio: false,   // bool      — a script produced output audio
  fileName:         '',       // string    — loaded file basename

  isRecording:           false,    // bool      — live capture is active
  isAnalyzing:           false,    // bool      — a Praat job is running
  isDownloadingScripts:  false,    // bool      — community scripts downloading
  isPlaying:             false,    // bool      — transport is playing
  playingSource:    'none',   // 'original' | 'processed' | 'none'

  praatFound:       false,    // bool      — Praat executable was located

  status:           'Loading…',
  // 'idle' | 'recording' | 'analyzing' | 'playing' | 'error'
  statusType:       'idle',

  // null means no analysis has run yet.
  // Shape: { pairs: [['key', 'value'], ...], raw: string }
  results:          null,

  waveformSamples:  [],       // float[]   — downsampled original waveform (~512pts)
  processedSamples: [],       // float[]   — downsampled processed waveform

  playbackFraction: 0,        // 0–1       — cursor position in the playing clip

  scriptParams:     [],       // FormParam[]  — extra form fields for current script

  // Populated only in debug builds (-DPRAATPLUGIN_DEBUG_LOGGING=ON).
  // Shape: { latencyMs: number, logPath: string, entries: {t,msg,kind}[] }
  debug:            null,
}

// ── Hook ──────────────────────────────────────────────────────────────────────
export function usePluginState() {
  const [state, setState] = useState(
    // Use mock data when running outside JUCE so the UI is always visible.
    typeof window.__JUCE__ !== 'undefined' ? INITIAL_STATE : MOCK_STATE
  )

  useEffect(() => {
    // Ask C++ for the current state the moment the UI mounts.
    sendToPlugin('requestState')

    // C++ pushes incremental real-time updates at 20fps (no scriptFolders here).
    const cleanupState = onPluginEvent('stateUpdate', (incoming) => {
      // Normalise array fields: a default-constructed juce::var serialises as
      // JSON null, not [].  The UI components always expect real arrays.
      if (incoming.waveformSamples  == null) incoming.waveformSamples  = []
      if (incoming.processedSamples == null) incoming.processedSamples = []

      // _debug is only present in debug builds; map it to `debug` and strip the
      // underscore-prefixed key so it doesn't clutter the rest of the state.
      const debug = incoming._debug ?? null
      const { _debug: _ignored, ...rest } = incoming
      setState(prev => ({ ...prev, ...rest, debug }))
    })

    // Script list is sent as its own event — only fires when the list changes.
    // This keeps the heavy 400-item payload out of the 20fps stateUpdate stream.
    const cleanupScripts = onPluginEvent('scriptsUpdate', (folders) => {
      setState(prev => ({ ...prev, scriptFolders: folders ?? [] }))
    })

    return () => { cleanupState(); cleanupScripts() }
  }, [])

  // ── Action creators ─────────────────────────────────────────────────────
  // Thin wrappers so components stay completely decoupled from the bridge.
  const actions = {
    loadAudioFile:    useCallback(() => sendToPlugin('loadAudioFile'),        []),
    toggleRecord:     useCallback(() => sendToPlugin('toggleRecord'),         []),
    playOriginal:     useCallback(() => sendToPlugin('playOriginal'),         []),
    playProcessed:    useCallback(() => sendToPlugin('playProcessed'),        []),
    stopPlayback:     useCallback(() => sendToPlugin('stopPlayback'),         []),
    loadScriptsDir:   useCallback(() => sendToPlugin('loadScriptsDir'),       []),
    downloadScripts:  useCallback(() => sendToPlugin('downloadScripts'),      []),
    analyze:          useCallback(() => sendToPlugin('analyze'),              []),
    cancelAnalysis:   useCallback(() => sendToPlugin('cancelAnalysis'),       []),
    selectScript:     useCallback(
      (scriptName) => sendToPlugin('selectScript', { name: scriptName }),     []
    ),
    setScriptParam:   useCallback(
      (name, value) => sendToPlugin('setScriptParam', { name, value }),         []
    ),
    // Send selected region as 0–1 fractions; C++ converts to seconds.
    setRegion:        useCallback(
      (start, end) => sendToPlugin('setRegion', { startFraction: start, endFraction: end }), []
    ),
    exportProcessed:       useCallback(() => sendToPlugin('exportProcessed'),        []),
    browsePraatExecutable: useCallback(() => sendToPlugin('browsePraatExecutable'), []),
  }

  return { state, actions }
}

// ── Mock state for npm run dev ────────────────────────────────────────────────
// Makes every component render with realistic placeholder data so you can
// work on the UI without having Ableton open.
const MOCK_STATE = {
  ...INITIAL_STATE,
  scriptFolders: [
    { name: 'BUNDLED',    scripts: ['pitch_analysis', 'formant_analysis', 'robot_voice'] },
    { name: 'PITCH',      scripts: ['pitch_shift', 'pitch_correction', 'vibrato', 'pitch_morphing'] },
    { name: 'REVERB',     scripts: ['shimmer_reverb', 'convolution_reverb', 'room_simulation'] },
    { name: 'DISTORTION', scripts: ['fuzz_distortion', 'soft_clip', 'tanh_saturation'] },
  ],
  selectedScript:   'BUNDLED/pitch_analysis',
  hasAudio:         true,
  fileName:         'vocal_take_01.wav',
  praatFound:       true,
  status:           'Ready — press Run to execute the selected script.',
  statusType:       'idle',
  results: {
    pairs: [
      ['mean_pitch',  '220.5 Hz'],
      ['max_pitch',   '350.2 Hz'],
      ['voiced_frac', '0.82'],
    ],
    raw: '',
  },
  scriptParams: [
    { name: 'stretchFactor', type: 'real',     default: '2.0',  value: '2.0',  options: [] },
    { name: 'windowSize',    type: 'positive', default: '0.25', value: '0.25', options: [] },
  ],
  debug: {
    latencyMs: 4,
    logPath: 'C:\\Users\\dev\\AppData\\Local\\Temp\\PraatPlugin\\debug.log',
    entries: [
      { t: '14:32:06.001', msg: 'Script completed: pitch_analysis.praat',            kind: 'info'  },
      { t: '14:32:05.880', msg: 'Running script: pitch_analysis.praat',              kind: 'info'  },
      { t: '14:32:05.210', msg: 'loadAudioFromFile (file picker)  245 ms',           kind: 'slow'  },
      { t: '14:32:04.800', msg: 'STALL END — total duration 380 ms',                 kind: 'stall' },
      { t: '14:32:04.450', msg: 'STALL BEGIN — message thread unresponsive 320 ms',  kind: 'stall' },
      { t: '14:32:04.100', msg: 'PraatRunner: Praat exited with code 1 — ...',       kind: 'error' },
      { t: '14:32:03.500', msg: 'setStateInformation called',                        kind: 'info'  },
      { t: '14:32:03.200', msg: 'installBundledScripts  12 ms',                      kind: 'info'  },
      { t: '14:32:03.100', msg: 'Processor constructed',                             kind: 'info'  },
    ],
  },
}
