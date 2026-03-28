// ─── ScriptParams ─────────────────────────────────────────────────────────────
//
// Renders editable controls for the extra form parameters of the active Praat
// script (every field beyond inputFile / outputFile).
//
// Supported Praat types → control:
//   real, positive, natural, integer  →  range slider + number input
//   boolean                           →  ON / OFF toggle button
//   choice, optionmenu                →  select dropdown
//   sentence, word, text              →  text input
//
// Hidden when there are no extra parameters.
// ─────────────────────────────────────────────────────────────────────────────

import './ScriptParams.css'

export function ScriptParams ({ params, onParamChange }) {
  if (!params || params.length === 0) return null

  return (
    <div className="script-params">
      <span className="section-label script-params__label">PARAMETERS</span>
      <div className="script-params__list">
        {params.map(param => (
          <ParamRow key={param.name} param={param} onChange={onParamChange} />
        ))}
      </div>
    </div>
  )
}

// ── Individual parameter row ──────────────────────────────────────────────────

function ParamRow ({ param, onChange }) {
  const { name, type, value, options } = param

  // Boolean → ON / OFF toggle
  if (type === 'boolean') {
    return (
      <div className="script-params__row">
        <span className="script-params__name">{name}</span>
        <button
          className={`btn script-params__toggle ${value === '1' ? 'script-params__toggle--on' : ''}`}
          onClick={() => onChange(name, value === '1' ? '0' : '1')}
        >
          {value === '1' ? 'ON' : 'OFF'}
        </button>
      </div>
    )
  }

  // Choice / optionmenu → select dropdown
  if (type === 'choice' || type === 'optionmenu') {
    // Praat uses 1-based indices; convert to 0-based for the select element.
    const selectedIdx = Math.max(0, parseInt(value, 10) - 1)
    return (
      <div className="script-params__row">
        <span className="script-params__name">{name}</span>
        <select
          className="script-params__select"
          value={selectedIdx}
          onChange={e => onChange(name, String(parseInt(e.target.value, 10) + 1))}
        >
          {(options || []).map((opt, i) => (
            <option key={i} value={i}>{opt}</option>
          ))}
        </select>
      </div>
    )
  }

  // String types → text input
  if (type === 'sentence' || type === 'word' || type === 'text') {
    return (
      <div className="script-params__row">
        <span className="script-params__name">{name}</span>
        <input
          className="script-params__input"
          type="text"
          value={value}
          onChange={e => onChange(name, e.target.value)}
        />
      </div>
    )
  }

  // Numeric (real, positive, natural, integer) → slider + number input
  const range = numericRange(type, param.default)

  return (
    <div className="script-params__row script-params__row--numeric">
      <span className="script-params__name">{name}</span>
      <input
        type="range"
        className="script-params__slider"
        min={range.min}
        max={range.max}
        step={range.step}
        value={clamp(parseFloat(value) || 0, range.min, range.max)}
        onChange={e => onChange(name, e.target.value)}
      />
      <input
        type="number"
        className="script-params__number"
        value={value}
        step={range.step}
        onChange={e => onChange(name, e.target.value)}
      />
    </div>
  )
}

// ── Helpers ───────────────────────────────────────────────────────────────────

function clamp (v, lo, hi) { return Math.min(hi, Math.max(lo, v)) }

function numericRange (type, defaultVal) {
  const n    = parseFloat(defaultVal) || 0
  const isInt = type === 'integer' || type === 'natural'
  const min  = type === 'natural' ? 1 : type === 'positive' ? 0.001 : 0
  const step = isInt ? 1
             : Math.abs(n) <= 0.01 ? 0.001
             : Math.abs(n) <= 1    ? 0.01
             :                       0.1
  const max  = n <= 0 ? (isInt ? 20 : 10) : Math.max(isInt ? 20 : 10, n * 4)
  return { min, max, step }
}
