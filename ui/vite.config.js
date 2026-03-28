import { defineConfig }   from 'vite'
import react               from '@vitejs/plugin-react'
import { viteSingleFile }  from 'vite-plugin-singlefile'
import fs                  from 'node:fs'
import path                from 'node:path'

// ── stripModuleType ────────────────────────────────────────────────────────────
// WebKit on macOS blocks <script type="module"> when the page is loaded from a
// custom URL scheme (juce://juce.backend/).
//
// viteSingleFile inlines all scripts as type="module" inline tags.
// transformIndexHtml runs before the inlining, so we use closeBundle instead —
// it fires after every plugin has written its output — then patch dist/index.html
// directly on disk to turn every inline module script into a plain script.
function stripModuleType() {
  return {
    name: 'strip-module-type',
    apply: 'build',
    closeBundle() {
      const htmlPath = path.resolve('dist', 'index.html')
      if (!fs.existsSync(htmlPath)) return

      const before = fs.readFileSync(htmlPath, 'utf8')
      const after  = before
        .replace(/<script type="module" crossorigin>/g, '<script>')
        .replace(/<script type="module">/g,             '<script>')

      fs.writeFileSync(htmlPath, after)
      console.log('[strip-module-type] Removed type="module" from inline scripts ✓')
    },
  }
}

// Produces a single self-contained index.html with all CSS and JS inlined.
// JUCE's ResourceProvider serves this file from BinaryData — no disk writes,
// no network requests, no CDN dependencies.
export default defineConfig({
  plugins: [
    react(),
    viteSingleFile(),    // inlines all assets into index.html at build time
    stripModuleType(),   // removes type="module" for WebKit custom-scheme compat
  ],
  build: {
    outDir: 'dist',
    assetsInlineLimit: Infinity,
    cssCodeSplit: false,
  },
  base: './',
})
