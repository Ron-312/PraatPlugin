// ─── StatusBar ────────────────────────────────────────────────────────────────
//
// Thin bar at the very bottom of the plugin window.
//
// Shows a square LED indicator and a monospace status message.
// The LED colour communicates mode at a glance:
//   idle      → dim grey
//   recording → red (blinking)
//   analyzing → signal-pink (blinking)
//   playing   → white
//   error     → red (solid)
// ─────────────────────────────────────────────────────────────────────────────

import './StatusBar.css'

// Maps statusType to a CSS modifier class on the LED.
const LED_MODIFIER = {
  idle:      'status-bar__led--idle',
  recording: 'status-bar__led--recording',
  analyzing: 'status-bar__led--analyzing',
  playing:   'status-bar__led--playing',
  error:     'status-bar__led--error',
}

export function StatusBar({ status, statusType }) {
  const ledClass = LED_MODIFIER[statusType] ?? LED_MODIFIER.idle

  return (
    <div className="status-bar">
      <span className={`status-bar__led ${ledClass}`} aria-hidden="true" />
      <span className="status-bar__message">{status}</span>
    </div>
  )
}
