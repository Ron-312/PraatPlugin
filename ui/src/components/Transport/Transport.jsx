// ─── Transport ────────────────────────────────────────────────────────────────
//
// Playback controls: Play A (original), Play B (processed), Stop.
//
// The active-playing button is highlighted white so the user always knows
// which source is currently playing without needing a separate indicator.
// ─────────────────────────────────────────────────────────────────────────────

import './Transport.css'

export function Transport({
  isPlaying,
  playingSource,    // 'original' | 'processed' | 'none'
  hasAudio,
  hasProcessedAudio,
  onPlayOriginal,
  onPlayProcessed,
  onStopPlayback,
  onExportProcessed,
  onStartDragExport,
}) {
  const playingOriginal  = isPlaying && playingSource === 'original'
  const playingProcessed = isPlaying && playingSource === 'processed'

  return (
    <div className="transport">
      <button
        className={`btn transport__btn ${playingOriginal ? 'transport__btn--active' : ''}`}
        onClick={onPlayOriginal}
        disabled={!hasAudio}
      >
        ▶ PLAY A
      </button>

      <button
        className={`btn transport__btn ${playingProcessed ? 'transport__btn--active' : ''}`}
        onClick={onPlayProcessed}
        disabled={!hasProcessedAudio}
      >
        ▶ PLAY B
      </button>

      <button
        className="btn transport__btn"
        onClick={onStopPlayback}
        disabled={!isPlaying}
      >
        ■ STOP
      </button>

      {/* Export group: click to save file, mousedown-drag to drop into DAW */}
      <div className="transport__export-group">
        <button
          className="btn transport__export-btn"
          onClick={onExportProcessed}
          disabled={!hasProcessedAudio}
          title="Save processed audio to disk"
        >
          ↓ EXPORT
        </button>
        <button
          className={`btn transport__drag-btn ${!hasProcessedAudio ? 'transport__drag-btn--disabled' : ''}`}
          onMouseDown={hasProcessedAudio ? onStartDragExport : undefined}
          disabled={!hasProcessedAudio}
          title="Drag processed audio into DAW"
        >
          ⠿
        </button>
      </div>
    </div>
  )
}
