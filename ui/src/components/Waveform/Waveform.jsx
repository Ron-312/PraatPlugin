// ─── Waveform ─────────────────────────────────────────────────────────────────
//
// Canvas-based waveform display with optional drag-to-select region.
//
// Props:
//   samples          float[]  — RMS values from C++ (null → treated as [])
//   color            string   — literal hex colour (CSS vars NOT valid for canvas)
//   playbackFraction number   — 0–1 cursor position; 0 = hidden
//   selectionFraction {start,end} | null — highlighted region (0–1 fractions)
//   onRegionChange   (start, end) => void — if provided, enables drag-to-select
// ─────────────────────────────────────────────────────────────────────────────

import { useEffect, useRef } from 'react'
import './Waveform.css'

export function Waveform({
  label,
  samples,
  color,
  height = 72,
  isEmpty = false,
  playbackFraction = 0,
  selectionFraction = null,
  onRegionChange = null,
}) {
  const canvasRef = useRef(null)
  const dragRef   = useRef({ active: false, start: 0 })

  // Keep a ref to the latest draw params so pointer handlers can use them
  // without going through React state (keeps drag smooth at 60fps).
  const paramsRef = useRef({})
  paramsRef.current = { samples, color, height, isEmpty, playbackFraction, selectionFraction }

  function doDraw(selOverride) {
    const canvas = canvasRef.current
    if (!canvas) return
    const { samples: s, color: c, height: h, isEmpty: e,
            playbackFraction: pf, selectionFraction: sf } = paramsRef.current
    const w = canvas.offsetWidth
    if (w === 0) return
    canvas.width  = w
    canvas.height = h
    const ctx = canvas.getContext('2d')
    drawWaveform(ctx, w, h, s ?? [], c, e, pf,
                 selOverride !== undefined ? selOverride : sf)
  }

  // Redraw when any visual prop changes.
  useEffect(() => {
    doDraw(undefined)
  }, [samples, color, height, isEmpty, playbackFraction, selectionFraction])

  // Redraw on resize — uses paramsRef so no deps needed here.
  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const observer = new ResizeObserver(() => doDraw(undefined))
    observer.observe(canvas)
    return () => observer.disconnect()
  }, [])

  // ── Pointer handlers for drag-to-select ───────────────────────────────────

  function handlePointerDown(e) {
    const rect = e.currentTarget.getBoundingClientRect()
    const f    = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width))
    dragRef.current = { active: true, start: f }
    e.currentTarget.setPointerCapture(e.pointerId)
  }

  function handlePointerMove(e) {
    if (!dragRef.current.active) return
    const rect = e.currentTarget.getBoundingClientRect()
    const f    = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width))
    const sel  = mkSel(dragRef.current.start, f)
    doDraw(sel)   // draw directly — no React re-render, stays smooth
  }

  function handlePointerUp(e) {
    if (!dragRef.current.active) return
    dragRef.current.active = false
    const rect = e.currentTarget.getBoundingClientRect()
    const f    = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width))
    const { start } = dragRef.current
    // A click with no drag (<1% width) clears the selection → full file
    if (Math.abs(f - start) < 0.01) {
      onRegionChange(0, 1)
    } else {
      const sel = mkSel(start, f)
      onRegionChange(sel.start, sel.end)
    }
  }

  const screenStyle = {
    height,
    cursor: onRegionChange ? 'col-resize' : 'default',
  }

  return (
    <div className="waveform">
      {label && <span className="waveform__label section-label">{label}</span>}
      <div
        className="waveform__screen"
        style={screenStyle}
        onPointerDown={onRegionChange ? handlePointerDown : undefined}
        onPointerMove={onRegionChange ? handlePointerMove : undefined}
        onPointerUp={onRegionChange   ? handlePointerUp   : undefined}
      >
        <canvas ref={canvasRef} className="waveform__canvas" />
      </div>
    </div>
  )
}

// ── Helpers ───────────────────────────────────────────────────────────────────

function mkSel(a, b) {
  return { start: Math.min(a, b), end: Math.max(a, b) }
}

// ── Drawing ───────────────────────────────────────────────────────────────────

function drawWaveform(ctx, width, height, samples, color, isEmpty, playbackFraction, selectionFraction) {
  // Background
  ctx.fillStyle = '#111111'
  ctx.fillRect(0, 0, width, height)

  // Centre axis
  ctx.strokeStyle = '#2a2a2a'
  ctx.lineWidth = 1
  ctx.beginPath()
  ctx.moveTo(0, height / 2)
  ctx.lineTo(width, height / 2)
  ctx.stroke()

  if (isEmpty || samples.length === 0) {
    drawEmptyPlaceholder(ctx, width, height)
  } else {
    drawSignal(ctx, width, height, samples, color)
  }

  // Selection region overlay — drawn before cursor so cursor sits on top
  if (selectionFraction && selectionFraction.end > selectionFraction.start + 0.001) {
    const x1 = selectionFraction.start * width
    const x2 = selectionFraction.end   * width
    ctx.fillStyle = 'rgba(255, 255, 255, 0.10)'
    ctx.fillRect(x1, 0, x2 - x1, height)
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.45)'
    ctx.lineWidth = 1
    ctx.beginPath()
    ctx.moveTo(x1, 0); ctx.lineTo(x1, height)
    ctx.moveTo(x2, 0); ctx.lineTo(x2, height)
    ctx.stroke()
  }

  // Playback cursor — bright white line on top of everything
  if (playbackFraction > 0 && playbackFraction <= 1) {
    const x = Math.round(playbackFraction * width)
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.85)'
    ctx.lineWidth = 1.5
    ctx.beginPath()
    ctx.moveTo(x, 0)
    ctx.lineTo(x, height)
    ctx.stroke()
  }
}

// Waveform drawn as a mirrored filled area.
// color must be a literal hex — CSS vars are not resolved by Canvas 2D.
function drawSignal(ctx, width, height, samples, color) {
  const midY = height / 2
  const step = samples.length / width

  ctx.fillStyle   = color + '55'  // 8-digit hex: ~33% opacity
  ctx.strokeStyle = color
  ctx.lineWidth   = 1

  ctx.beginPath()
  for (let x = 0; x < width; x++) {
    const rms = samples[Math.floor(x * step)] ?? 0
    const y   = midY - rms * midY
    x === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y)
  }
  for (let x = width - 1; x >= 0; x--) {
    const rms = samples[Math.floor(x * step)] ?? 0
    const y   = midY + rms * midY
    ctx.lineTo(x, y)
  }
  ctx.closePath()
  ctx.fill()

  ctx.beginPath()
  for (let x = 0; x < width; x++) {
    const rms = samples[Math.floor(x * step)] ?? 0
    const y   = midY - rms * midY
    x === 0 ? ctx.moveTo(x, y) : ctx.lineTo(x, y)
  }
  ctx.stroke()
}

function drawEmptyPlaceholder(ctx, width, height) {
  ctx.fillStyle = '#333333'
  ctx.font      = '9px Menlo, Monaco, monospace'
  ctx.textAlign = 'center'
  ctx.fillText('NO AUDIO', width / 2, height / 2 + 3)
}
