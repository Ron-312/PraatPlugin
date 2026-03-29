// ─── AnalyzeButton ────────────────────────────────────────────────────────────
//
// The hero action button of the plugin — the one thing users press to do work.
//
// While analyzing:
//   - Becomes a pink cancel button with a sweeping indeterminate progress bar.
//   - Shows elapsed time so the user knows how long the script has been running.
//   - Praat scripts are black-box processes — we can't know total duration,
//     so the bar is indeterminate rather than fake-progress.
// ─────────────────────────────────────────────────────────────────────────────

import { useState, useEffect } from 'react'
import './AnalyzeButton.css'

export function AnalyzeButton({ isRunning, canRun, onRun, onCancel }) {
  const [elapsed, setElapsed] = useState(0)

  useEffect(() => {
    if (!isRunning) { setElapsed(0); return }
    const id = setInterval(() => setElapsed(s => s + 1), 1000)
    return () => clearInterval(id)
  }, [isRunning])

  const mm = Math.floor(elapsed / 60)
  const ss = String(elapsed % 60).padStart(2, '0')
  const elapsedStr = `${mm}:${ss}`

  if (isRunning) {
    return (
      <div className="analyze-button">
        <button
          className="analyze-button__btn analyze-button__btn--cancel"
          onClick={onCancel}
        >
          <span className="analyze-button__sweep" />
          <span className="analyze-button__label">{elapsedStr} &nbsp;·&nbsp; ■ CANCEL</span>
        </button>
      </div>
    )
  }

  return (
    <div className="analyze-button">
      <button
        className="analyze-button__btn"
        onClick={onRun}
        disabled={!canRun}
      >
        RUN
      </button>
    </div>
  )
}
