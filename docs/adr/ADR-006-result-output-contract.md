# ADR-006: Praat Script Output Uses a KEY: value Line Convention

**Date:** 2026-03-25
**Status:** Accepted
**Deciders:** Project founder

---

## Context

When `PraatRunner` launches Praat with `--run`, any output from `writeInfoLine:` or `appendInfoLine:` in the script goes to stdout. The plugin captures this stdout and must parse it into structured results that can be displayed in the UI.

The question is: what format should scripts use to communicate results back to the plugin?

Options range from fully structured (JSON, CSV, XML) to completely freeform (raw text).

The audience writing scripts includes:
- Phoneticians and linguists who know Praat scripting but are not programmers
- Researchers adapting existing Praat scripts
- People who want to drop in `.praat` scripts they've downloaded from the internet

## Decision

Scripts communicate structured results via lines of the form:
```
KEY: value
```

Example output from a pitch analysis script:
```
mean_pitch: 220.5 Hz
min_pitch: 87.3 Hz
max_pitch: 441.0 Hz
voiced_frames: 82%
duration: 1.432 s
```

Rules:
- `KEY` is any string without a colon
- A colon followed by a space (`: `) separates key from value
- `value` is the rest of the line (can contain spaces, units, colons)
- Lines that do not match `KEY: value` are stored as **raw text** and shown verbatim — they do not cause errors
- The parser is lenient: unrecognised or malformed lines are never rejected

`ResultParser` extracts all matching `KEY: value` pairs into a `juce::StringPairArray` and stores everything else in `rawPraatOutput`. The UI displays key-value pairs in a structured table and raw text in a scrollable log.

## Rationale

**Why not JSON:**
Praat's scripting language does not have built-in JSON serialization. Generating valid JSON from Praat would require the script author to manually escape strings, handle arrays, and close braces correctly — fragile and unfriendly to non-programmers.

**Why not CSV:**
CSV requires a fixed schema (header row + data row) and doesn't handle heterogeneous metrics (pitch, duration, formants) well in a single output. It also requires quoting rules that are easy to get wrong.

**Why not raw text with no convention:**
The plugin would have no way to display results in a structured table. The UI would just be a text dump, which is less useful.

**Why KEY: value:**
- Natural in Praat: `writeInfoLine: "mean_pitch: " + string$(mean) + " Hz"` reads clearly
- Lenient: wrong lines don't break anything
- Extensible: scripts can add new metrics without changing the plugin
- Familiar: matches the style of many Unix tools (`git config --list`, HTTP headers, `.env` files)
- No quoting needed: values are the rest of the line

## Alternatives Considered

| Option | Why not chosen |
|--------|---------------|
| JSON output | Hard to generate correctly in Praat scripting; unfriendly to domain experts |
| CSV | Requires fixed schema; multiline or heterogeneous output is awkward |
| XML | Even more verbose and error-prone to generate manually in Praat |
| Strict format (reject non-conforming lines) | Would break many existing Praat scripts that print informational text |

## Consequences

**Positive:**
- Existing Praat scripts that use `writeInfoLine:` for informational output work out of the box (their lines go to raw text, no errors)
- Script authors add structured output with one line change: `writeInfoLine: "KEY: value"`
- Lenient parsing means the plugin is maximally compatible with real-world scripts

**Negative / Trade-offs:**
- No type information: values are always strings. If the UI needs to plot a time series, the script must produce multiple `time_N: value` pairs or a different convention.
- No nested structure: `KEY: value` can't express objects or arrays natively. A script producing formant data for F1–F4 would need `f1: 800 Hz`, `f2: 1200 Hz`, etc.
- Key collisions: if a script prints the same key twice, the second value overwrites the first in `StringPairArray`.

**Risks and mitigations:**
- Risk: Users complain that their scripts print debug text and it appears in the "Raw output" panel unexpectedly.
  Mitigation: Label the raw output panel clearly as "Script console output" and document the KEY: value convention in CONTRIBUTING.md.
- Risk: The convention is not enforced, so users might expect structured results and get only raw text if they forget the convention.
  Mitigation: Bundled example scripts demonstrate the convention. The UI shows a hint if no KEY: value pairs are found but raw text is present.
