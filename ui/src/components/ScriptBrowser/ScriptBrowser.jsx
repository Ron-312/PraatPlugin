// ─── ScriptBrowser ────────────────────────────────────────────────────────────
//
// A full-screen overlay modal for browsing and selecting Praat scripts.
//
// Layout:
//   ┌─────────────────────────────────────────────────────┐
//   │  🔍 Search scripts…                      [✕ Close]  │
//   ├──────────────────┬──────────────────────────────────┤
//   │ ALL (312)        │  pitch_shift          PITCH      │
//   │ ANALYSIS (33)    │  pitch_correction     PITCH      │
//   │ PITCH (26)   ◄   │  vibrato              PITCH      │
//   │ DISTORTION (15)  │  ...                             │
//   │ ...              │                                  │
//   └──────────────────┴──────────────────────────────────┘
//
// Keyboard: Esc closes. Click outside the modal closes it.
// ─────────────────────────────────────────────────────────────────────────────

import { useState, useEffect, useRef } from 'react'
import './ScriptBrowser.css'

export function ScriptBrowser({ scriptFolders, selectedScript, onSelect, onClose }) {
  const [search, setSearch]       = useState('')
  const [activeFolder, setFolder] = useState('ALL')
  const searchRef                 = useRef(null)

  // Flatten all scripts into a searchable list: [{qualified, folder, name}]
  const allScripts = scriptFolders.flatMap((f) =>
    f.scripts.map((s) => ({ qualified: `${f.name}/${s}`, folder: f.name, name: s }))
  )

  const visibleScripts = allScripts.filter(({ folder, name }) => {
    const matchesFolder = activeFolder === 'ALL' || folder === activeFolder
    const matchesSearch = search === '' || name.toLowerCase().includes(search.toLowerCase())
    return matchesFolder && matchesSearch
  })

  // Close on Esc
  useEffect(() => {
    function onKeyDown(e) {
      if (e.key === 'Escape') onClose()
    }
    window.addEventListener('keydown', onKeyDown)
    return () => window.removeEventListener('keydown', onKeyDown)
  }, [onClose])

  // Auto-focus search input when the browser opens
  useEffect(() => {
    searchRef.current?.focus()
  }, [])

  // When the user switches folder, reset search so the list isn't confusing
  function handleFolderClick(folderName) {
    setFolder(folderName)
    setSearch('')
    searchRef.current?.focus()
  }

  return (
    <div className="script-browser-overlay" onMouseDown={onClose}>
      {/* stopPropagation prevents the overlay click-to-close from firing when
          clicking inside the panel itself */}
      <div className="script-browser" onMouseDown={(e) => e.stopPropagation()}>

        {/* ── Header: search + close ──────────────────────────────────────── */}
        <div className="script-browser__header">
          <span className="script-browser__search-icon" aria-hidden="true">⌕</span>
          <input
            ref={searchRef}
            className="script-browser__search"
            placeholder="Search scripts…"
            value={search}
            onChange={(e) => { setSearch(e.target.value); setFolder('ALL') }}
            spellCheck={false}
          />
          <button className="script-browser__close" onClick={onClose} title="Close (Esc)">
            ✕
          </button>
        </div>

        {/* ── Body: folders (left) + scripts (right) ─────────────────────── */}
        <div className="script-browser__body">

          {/* Left panel — folder list */}
          <div className="script-browser__folders">
            <button
              className={`script-browser__folder-item ${activeFolder === 'ALL' ? 'active' : ''}`}
              onClick={() => handleFolderClick('ALL')}
            >
              <span className="script-browser__folder-name">ALL</span>
              <span className="script-browser__count">{allScripts.length}</span>
            </button>

            {scriptFolders.map((f) => (
              <button
                key={f.name}
                className={`script-browser__folder-item ${activeFolder === f.name ? 'active' : ''}`}
                onClick={() => handleFolderClick(f.name)}
              >
                <span className="script-browser__folder-name">{f.name}</span>
                <span className="script-browser__count">{f.scripts.length}</span>
              </button>
            ))}
          </div>

          {/* Right panel — script list */}
          <div className="script-browser__scripts">
            {visibleScripts.length === 0 && (
              <span className="script-browser__empty">No scripts match</span>
            )}

            {visibleScripts.map(({ qualified, folder, name }) => (
              <button
                key={qualified}
                className={`script-browser__script-item ${selectedScript === qualified ? 'active' : ''}`}
                onClick={() => onSelect(qualified)}
              >
                <span className="script-browser__script-name">{name}</span>
                {/* Show folder badge when browsing ALL or searching across folders */}
                {(activeFolder === 'ALL' || search !== '') && (
                  <span className="script-browser__script-folder">{folder}</span>
                )}
              </button>
            ))}
          </div>

        </div>
      </div>
    </div>
  )
}
