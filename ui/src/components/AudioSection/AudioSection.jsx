// ─── AudioSection ─────────────────────────────────────────────────────────────
//
// Controls for loading and recording audio.
// Shows the loaded file name and two waveform canvases (original + processed).
//
// Waveform components live here (not in App) because they conceptually belong
// to "audio source" — they visualise the audio that was loaded or recorded.
// ─────────────────────────────────────────────────────────────────────────────

import { Waveform } from '../Waveform/Waveform'
import './AudioSection.css'

export function AudioSection({
  fileName,
  isRecording,
  hasProcessedAudio,
  waveformSamples,
  processedSamples,
  playingSource,
  playbackFraction,
  selectionFraction,
  onRegionChange,
  onLoadAudioFile,
  onToggleRecord,
  onStartDragExport,
  isDragOver,
  showSoundB,
  soundBFileName,
  onLoadSoundB,
  onClearSoundB,
}) {
  return (
    <section className="audio-section">
      <div className="audio-section__controls">
        <button
          className={`btn ${isDragOver ? 'audio-section__load-btn--drop' : ''}`}
          onClick={onLoadAudioFile}
          title={isDragOver ? 'Drop audio file here' : 'Load audio file'}
        >
          {isDragOver ? 'v DROP HERE' : 'LOAD FILE'}
        </button>

        <button
          className={`btn audio-section__rec-btn ${isRecording ? 'audio-section__rec-btn--active' : ''}`}
          onClick={onToggleRecord}
        >
          {/* Blinking dot when recording */}
          <span className={`audio-section__rec-dot ${isRecording ? 'audio-section__rec-dot--live' : ''}`} />
          {isRecording ? 'STOP REC' : 'REC'}
        </button>

        <span className="audio-section__file-name">
          {fileName || 'no file loaded'}
        </span>
      </div>

      {/* Second input for scripts that read two Sounds (cross-synthesis, morphing…).
          Sound A is the main take above; B goes in as the second selected Sound. */}
      {showSoundB && (
        <div className="audio-section__controls audio-section__sound-b">
          <button className="btn" onClick={onLoadSoundB} title="Second Sound for this script">
            LOAD SOUND B
          </button>
          <span className="audio-section__file-name">
            {soundBFileName || 'no Sound B (this script uses a second Sound)'}
          </span>
          {soundBFileName && (
            <button className="btn" onClick={onClearSoundB} title="Remove Sound B">✕</button>
          )}
        </div>
      )}

      <Waveform
        label="ORIGINAL"
        samples={waveformSamples}
        color="#e05090"
        height={72}
        playbackFraction={playingSource === 'original' ? playbackFraction : 0}
        selectionFraction={selectionFraction}
        onRegionChange={onRegionChange}
      />

      <Waveform
        label="PROCESSED"
        samples={hasProcessedAudio ? processedSamples : []}
        color="#a0a0a0"
        height={52}
        isEmpty={!hasProcessedAudio}
        playbackFraction={playingSource === 'processed' ? playbackFraction : 0}
        onDragExport={hasProcessedAudio ? onStartDragExport : null}
      />
    </section>
  )
}
