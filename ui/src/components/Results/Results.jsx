// ─── Results ──────────────────────────────────────────────────────────────────
//
// Displays the output of the most recently completed Praat analysis.
//
// Structured results (KEY: value pairs from the script's writeInfoLine output)
// are shown in a two-column layout: pink key, white value.
// Any unstructured console output appears below in dimmed monospace.
//
// This is the component the user reads most carefully — so legibility is the
// top priority: generous line-height, monospace font, no decorative noise.
// ─────────────────────────────────────────────────────────────────────────────

import './Results.css'

export function Results({ results }) {
  return (
    <div className="results">
      <span className="section-label results__label">RESULTS</span>

      <div className="results__screen">
        <ResultsContent results={results} />
      </div>
    </div>
  )
}

// ── Content states ────────────────────────────────────────────────────────────

function ResultsContent({ results }) {
  if (!results) {
    return <span className="results__placeholder">Run ANALYZE to see output here.</span>
  }

  const hasPairs = results.pairs && results.pairs.length > 0
  const hasRaw   = results.raw && results.raw.trim().length > 0

  if (!hasPairs && !hasRaw) {
    return <span className="results__placeholder">Script ran — no output was produced.</span>
  }

  return (
    <>
      {hasPairs && (
        <table className="results__table">
          <tbody>
            {results.pairs.map(([key, value]) => (
              <tr key={key} className="results__row">
                <td className="results__key">{key}</td>
                <td className="results__value">{value}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}

      {hasRaw && (
        <pre className="results__raw">{results.raw.trim()}</pre>
      )}
    </>
  )
}
