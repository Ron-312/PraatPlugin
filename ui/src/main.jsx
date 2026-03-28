import { StrictMode } from 'react'
import { createRoot } from 'react-dom/client'
import './styles/tokens.css'
import './styles/global.css'
import { App } from './App'

// viteSingleFile inlines the bundle as a <script> in <head>, which strips the
// implicit defer that type="module" provides.  Without defer, the script runs
// before <body> is parsed, so getElementById('root') returns null.
// Wrapping in DOMContentLoaded restores the same "wait for DOM" behaviour.
document.addEventListener('DOMContentLoaded', () => {
  createRoot(document.getElementById('root')).render(
    <StrictMode>
      <App />
    </StrictMode>
  )
})
