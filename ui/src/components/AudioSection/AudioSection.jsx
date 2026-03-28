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
}) {
  return (
    <section className="audio-section">
      <div className="audio-section__controls">
        <button className="btn" onClick={onLoadAudioFile}>
          LOAD FILE
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
      />
    </section>
  )
}
