// ─── App ──────────────────────────────────────────────────────────────────────
//
// Root component. Owns layout and wires state + actions into child components.
//
// All plugin state lives in usePluginState — App is deliberately thin:
// it reads state, passes slices to children, and passes action creators down.
// Components are unaware of the bridge or the processor.
//
// Layout (top → bottom, fixed height = plugin window):
//   Header       — identity + Praat status
//   AudioSection — load/record controls + waveforms
//   Transport    — play / stop
//   ScriptSection — script dropdown + browse
//   AnalyzeButton — the hero action
//   Results       — analysis output (flex: 1 — grows to fill remaining space)
//   StatusBar     — status LED + message
// ─────────────────────────────────────────────────────────────────────────────

import { usePluginState }    from './hooks/usePluginState'
import { Header }            from './components/Header/Header'
import { AudioSection }      from './components/AudioSection/AudioSection'
import { Transport }         from './components/Transport/Transport'
import { ScriptSection }     from './components/ScriptSection/ScriptSection'
import { ScriptParams }      from './components/ScriptParams/ScriptParams'
import { AnalyzeButton }     from './components/AnalyzeButton/AnalyzeButton'
import { Results }           from './components/Results/Results'
import { StatusBar }         from './components/StatusBar/StatusBar'
import { useState, useCallback } from 'react'
import './App.css'

export function App() {
  const { state, actions } = usePluginState()

  // Local state for the drag-selected region on the original waveform.
  // Kept here (not in C++ state) so the selection feels instant during drag.
  const [waveformSelection, setWaveformSelection] = useState(null)

  const handleRegionChange = useCallback((start, end) => {
    // A {0,1} range means "full file" — treat as clearing the selection.
    const isFull = start <= 0 && end >= 1
    setWaveformSelection(isFull ? null : { start, end })
    actions.setRegion(start, end)
  }, [actions])

  const handleParamChange = useCallback((name, value) => {
    actions.setScriptParam(name, value)
  }, [actions])

  // Morph is only meaningful when audio is loaded and a script is selected.
  const canAnalyze = state.hasAudio && state.selectedScript.length > 0

  return (
    <div className="app">
      <Header
        praatFound={state.praatFound}
        onBrowsePraat={actions.browsePraatExecutable}
      />

      <AudioSection
        fileName={state.fileName}
        isRecording={state.isRecording}
        hasProcessedAudio={state.hasProcessedAudio}
        waveformSamples={state.waveformSamples}
        processedSamples={state.processedSamples}
        playingSource={state.playingSource}
        playbackFraction={state.playbackFraction}
        selectionFraction={waveformSelection}
        onRegionChange={handleRegionChange}
        onLoadAudioFile={actions.loadAudioFile}
        onToggleRecord={actions.toggleRecord}
      />

      <Transport
        isPlaying={state.isPlaying}
        playingSource={state.playingSource}
        hasAudio={state.hasAudio}
        hasProcessedAudio={state.hasProcessedAudio}
        onPlayOriginal={actions.playOriginal}
        onPlayProcessed={actions.playProcessed}
        onStopPlayback={actions.stopPlayback}
        onExportProcessed={actions.exportProcessed}
      />

      <ScriptSection
        scriptFolders={state.scriptFolders}
        selectedScript={state.selectedScript}
        onSelectScript={actions.selectScript}
        onLoadScriptsDir={actions.loadScriptsDir}
        onDownloadScripts={actions.downloadScripts}
      />

      <ScriptParams
        params={state.scriptParams}
        onParamChange={handleParamChange}
      />

      <AnalyzeButton
        isAnalyzing={state.isAnalyzing}
        canAnalyze={canAnalyze}
        onAnalyze={actions.analyze}
        onCancel={actions.cancelAnalysis}
      />

      <Results
        results={state.results}
      />

      <StatusBar
        status={state.status}
        statusType={state.statusType}
        isDownloadingScripts={state.isDownloadingScripts}
      />
    </div>
  )
}
