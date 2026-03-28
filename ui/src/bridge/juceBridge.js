// ─── JUCE ↔ JavaScript Bridge ─────────────────────────────────────────────────
//
// All communication between the React UI and C++ flows through this module.
// Nothing else in the app imports from window.__JUCE__ directly — isolation
// here means the entire rest of the app is testable in a normal browser.
//
// Two channels:
//
//   JS → C++  (user actions)
//     sendToPlugin(eventId, data)
//     Calls window.__JUCE__.backend.emitEvent — maps to a withEventListener
//     registration in PraatPluginEditor.cpp.
//
//   C++ → JS  (state updates)
//     onPluginEvent(eventId, handler) → returns a cleanup function
//     Subscribes via window.__JUCE__.backend.addEventListener — maps to
//     emitEventIfBrowserIsVisible() calls in the C++ timer.
//
// When running outside JUCE (e.g. `npm run dev`):
//   - sendToPlugin  logs to the console and does nothing.
//   - onPluginEvent registers nothing and returns a no-op cleanup.
//   Mock state is injected separately in usePluginState so the UI still renders.
// ─────────────────────────────────────────────────────────────────────────────

const isRunningInsideJuce = () => typeof window.__JUCE__ !== 'undefined'

// Send a fire-and-forget event to C++.
// eventId must match a withEventListener() registration in PraatPluginEditor.cpp.
export function sendToPlugin(eventId, data = {}) {
  if (!isRunningInsideJuce()) {
    console.log(`[juceBridge] mock send → "${eventId}"`, data)
    return
  }
  window.__JUCE__.backend.emitEvent(eventId, data)
}

// Subscribe to an event emitted by C++ via emitEventIfBrowserIsVisible().
// Returns a cleanup function — call it inside a useEffect cleanup.
export function onPluginEvent(eventId, handler) {
  if (!isRunningInsideJuce()) {
    console.log(`[juceBridge] mock listener registered for "${eventId}"`)
    return () => {}   // no-op cleanup
  }

  // JUCE returns a removal token (not the handler itself) from addEventListener.
  const removalToken = window.__JUCE__.backend.addEventListener(eventId, handler)

  return () => window.__JUCE__.backend.removeEventListener(removalToken)
}
