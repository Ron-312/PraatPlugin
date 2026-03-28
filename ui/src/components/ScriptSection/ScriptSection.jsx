// ─── ScriptSection ────────────────────────────────────────────────────────────
//
// Script selector dropdown and directory browser button.
//
// The dropdown is the primary control — it lists all .praat scripts found
// in the loaded scripts directory. Selecting one sets it as the active script
// in C++ via the bridge.
// ─────────────────────────────────────────────────────────────────────────────

import './ScriptSection.css'

export function ScriptSection({ scripts, selectedScript, onSelectScript, onLoadScriptsDir }) {
  const hasScripts = scripts.length > 0

  function handleChange(event) {
    onSelectScript(event.target.value)
  }

  return (
    <div className="script-section">
      <span className="section-label script-section__label">SCRIPT</span>

      <div className="script-section__row">
        <div className="script-section__select-wrapper">
          <select
            className="script-section__select"
            value={selectedScript}
            onChange={handleChange}
            disabled={!hasScripts}
          >
            {!hasScripts && (
              <option value="">— no scripts loaded —</option>
            )}
            {scripts.map((name) => (
              <option key={name} value={name}>
                {name}
              </option>
            ))}
          </select>
          {/* Custom chevron — CSS-only, no icon lib needed */}
          <span className="script-section__chevron" aria-hidden="true">▾</span>
        </div>

        <button className="btn script-section__browse-btn" onClick={onLoadScriptsDir}>
          BROWSE
        </button>
      </div>
    </div>
  )
}
