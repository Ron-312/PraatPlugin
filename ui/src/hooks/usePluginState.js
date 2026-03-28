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
  scripts:          [],       // string[]  — file names without extension
  selectedScript:   '',       // string    — currently selected script name

  hasAudio:         false,    // bool      — an audio file is loaded
  hasProcessedAudio: false,   // bool      — a script produced output audio
  fileName:         '',       // string    — loaded file basename

  isRecording:      false,    // bool      — live capture is active
  isAnalyzing:      false,    // bool      — a Praat job is running
  isPlaying:        false,    // bool      — transport is playing
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

    // C++ pushes incremental updates at 20fps — merge them into local state.
    const cleanup = onPluginEvent('stateUpdate', (incoming) => {
      // Normalise array fields: a default-constructed juce::var serialises as
      // JSON null, not [].  The UI components always expect real arrays.
      if (incoming.waveformSamples  == null) incoming.waveformSamples  = []
      if (incoming.processedSamples == null) incoming.processedSamples = []
      if (incoming.scripts          == null) incoming.scripts          = []
      setState(prev => ({ ...prev, ...incoming }))
    })

    return cleanup
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
    analyze:          useCallback(() => sendToPlugin('analyze'),              []),
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
    exportProcessed:  useCallback(() => sendToPlugin('exportProcessed'),      []),
  }

  return { state, actions }
}

// ── Mock state for npm run dev ────────────────────────────────────────────────
// Makes every component render with realistic placeholder data so you can
// work on the UI without having Ableton open.
const MOCK_STATE = {
  ...INITIAL_STATE,
  scripts:          ['pitch_analysis', 'formant_analysis', 'robot_voice'],
  selectedScript:   'pitch_analysis',
  hasAudio:         true,
  fileName:         'vocal_take_01.wav',
  praatFound:       true,
  status:           'Ready — press Analyze to run the selected script.',
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
}
