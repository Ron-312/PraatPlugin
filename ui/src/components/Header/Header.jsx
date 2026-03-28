// ─── Header ───────────────────────────────────────────────────────────────────
//
// Top bar of the plugin window. Shows the plugin identity on the left and the
// Praat executable status on the right.
//
// The pink logo mark is the only place we use --color-signal in a structural
// element — everything else uses it only on the audio waveform signal line.
// ─────────────────────────────────────────────────────────────────────────────

import './Header.css'

export function Header({ praatFound }) {
  return (
    <header className="header">
      <div className="header__identity">
        <div className="header__logo-mark">P</div>
        <span className="header__name">PRAAT PLUGIN</span>
        <span className="header__version">1.0</span>
      </div>

      <div className="header__praat-status">
        <span className={`header__led ${praatFound ? 'header__led--ok' : 'header__led--error'}`} />
        <span className="header__status-text">
          {praatFound ? 'PRAAT FOUND' : 'PRAAT NOT FOUND'}
        </span>
      </div>
    </header>
  )
}
