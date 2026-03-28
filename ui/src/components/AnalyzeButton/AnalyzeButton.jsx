// ─── AnalyzeButton ────────────────────────────────────────────────────────────
//
// The hero action button of the plugin — the one thing users press to do work.
//
// Design intent (Teenage Engineering influence):
//   - Full width, white fill, black text. The most visually distinct element.
//   - Disabled when no audio or no script is loaded.
//   - Shows "ANALYZING…" with an animated pulse while a job is running.
//   - No gradients, no shadow — flat and intentional.
// ─────────────────────────────────────────────────────────────────────────────

import './AnalyzeButton.css'

export function AnalyzeButton({ isAnalyzing, canAnalyze, onAnalyze }) {
  const label    = isAnalyzing ? 'MORPHING…' : 'MORPH'
  const disabled = isAnalyzing || !canAnalyze

  return (
    <div className="analyze-button">
      <button
        className={`analyze-button__btn ${isAnalyzing ? 'analyze-button__btn--running' : ''}`}
        onClick={onAnalyze}
        disabled={disabled}
      >
        {label}
      </button>
    </div>
  )
}
