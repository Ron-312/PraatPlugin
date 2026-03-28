// ─── ScriptSection ────────────────────────────────────────────────────────────
//
// Shows the currently selected script as a clickable button that opens the
// ScriptBrowser popup. Also provides a download button (↓) to fetch the
// community script library from GitHub, and a BROWSE button to load a local
// folder.
// ─────────────────────────────────────────────────────────────────────────────

import { useState } from 'react'
import { ScriptBrowser } from '../ScriptBrowser/ScriptBrowser'
import './ScriptSection.css'

export function ScriptSection({
  scriptFolders,
  selectedScript,
  onSelectScript,
  onLoadScriptsDir,
  onDownloadScripts,
}) {
  const [browserOpen, setBrowserOpen] = useState(false)

  const hasScripts = scriptFolders.length > 0

  // "PITCH/pitch_shift" → display "pitch_shift" with tag "PITCH"
  const displayName = selectedScript
    ? selectedScript.split('/').pop()
    : '— no script selected —'

  const folderTag = selectedScript && selectedScript.includes('/')
    ? selectedScript.split('/')[0]
    : null

  function handleSelect(qualifiedName) {
    onSelectScript(qualifiedName)
    setBrowserOpen(false)
  }

  return (
    <div className="script-section">
      <span className="section-label script-section__label">SCRIPT</span>

      <div className="script-section__row">
        {/* Trigger button — opens the script browser popup */}
        <button
          className="script-section__trigger"
          onClick={() => setBrowserOpen(true)}
          disabled={!hasScripts}
          title={selectedScript || 'No script selected'}
        >
          <span className="script-section__name">{displayName}</span>
          {folderTag && (
            <span className="script-section__folder-tag">{folderTag}</span>
          )}
          <span className="script-section__chevron" aria-hidden="true">▾</span>
        </button>

        {/* Download button — fetches/updates community scripts from GitHub */}
        <button
          className="btn script-section__dl-btn"
          onClick={onDownloadScripts}
          title="Download / update scripts from GitHub"
        >
          ↓
        </button>

        {/* Browse button — loads a local folder */}
        <button className="btn script-section__browse-btn" onClick={onLoadScriptsDir}>
          BROWSE
        </button>
      </div>

      {browserOpen && (
        <ScriptBrowser
          scriptFolders={scriptFolders}
          selectedScript={selectedScript}
          onSelect={handleSelect}
          onClose={() => setBrowserOpen(false)}
        />
      )}
    </div>
  )
}
