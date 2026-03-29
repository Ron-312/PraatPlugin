// ─── DevPanel ─────────────────────────────────────────────────────────────────
//
// In-plugin DevTools overlay.  Toggle with  Ctrl+Shift+D  (wired in App.jsx).
//
// Only renders when the C++ debug build passes a `_debug` object in stateUpdate
// (i.e. when compiled with -DPRAATPLUGIN_DEBUG_LOGGING=ON).
// In production builds `debug` is null and this component returns null.
//
// Layout:
//   ┌─────────────────────────────────────────────────────┐
//   │ DevTools   MSG 4 ms            [copy log]  [✕]      │
//   ├─────────────────────────────────────────────────────┤
//   │ 14:32:05  loadAudioFromFile  245 ms          (slow) │
//   │ 14:32:04  STALL BEGIN — 350 ms              (stall) │
//   │ ...                                                  │
//   ├─────────────────────────────────────────────────────┤
//   │ log: C:\Users\...\Temp\PraatPlugin\debug.log        │
//   └─────────────────────────────────────────────────────┘
// ─────────────────────────────────────────────────────────────────────────────

import './DevPanel.css'

export function DevPanel ({ debug, onClose }) {
  if (!debug) return null

  const latency = debug.latencyMs ?? 0
  const latencyClass =
    latency < 30 ? 'ok' : latency < 100 ? 'warn' : 'bad'

  const entries = debug.entries ?? []

  const copyLog = () => {
    const text = entries
      .map(e => `${e.t}  [${e.kind.padEnd(5)}]  ${e.msg}`)
      .join('\n')

    // navigator.clipboard requires a secure context; fall back to execCommand
    // for WebView2 environments where the custom URL scheme may not qualify.
    if (navigator.clipboard) {
      navigator.clipboard.writeText(text).catch(() => execCommandCopy(text))
    } else {
      execCommandCopy(text)
    }
  }

  return (
    <div className="devpanel">
      <div className="devpanel__bar">
        <span className="devpanel__title">DevTools</span>
        <span className={`devpanel__latency devpanel__latency--${latencyClass}`}>
          MSG&nbsp;{latency}&nbsp;ms
        </span>
        <button className="devpanel__btn" onClick={copyLog}>copy log</button>
        <button className="devpanel__btn devpanel__btn--close" onClick={onClose}>✕</button>
      </div>

      <div className="devpanel__log">
        {entries.length === 0 ? (
          <div className="devpanel__empty">
            No events yet — slow ops (&gt;30 ms) and errors will appear here.
          </div>
        ) : (
          entries.map((e, i) => (
            <div key={i} className={`devpanel__entry devpanel__entry--${e.kind}`}>
              <span className="devpanel__t">{e.t}</span>
              <span className="devpanel__msg">{e.msg}</span>
            </div>
          ))
        )}
      </div>

      <div className="devpanel__footer">
        log: {debug.logPath}
      </div>
    </div>
  )
}

function execCommandCopy (text) {
  const el = document.createElement('textarea')
  el.value = text
  el.style.cssText = 'position:fixed;opacity:0;pointer-events:none'
  document.body.appendChild(el)
  el.select()
  document.execCommand('copy')
  document.body.removeChild(el)
}
