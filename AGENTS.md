# Agent notes (Infinite-Noise)

This GitHub repo is the public plugin; git/PRs are appropriate here. Items below are intended behaviour — do not open a PR that “fixes” them.

- Infinite-Noise: VCV Rack 2 (min 2.5.0), C++11.
- Do not change working behaviour unless asked or clearly buggy. Prefer small incremental changes.
- `process()` is the audio hot path. `processParams` every 256 cycles is intentional; `process()` uses `.act`, params copies `.req` → `.act`.
- For published modules, keep backward-compatible patch load; write the new form. Suggest `currentJson` bumps; do not apply them.
- `currentJson` is what new saves write. `jsonVersion` is loaded from the patch and can be any older value. `if (jsonVersion == N)` for N < `currentJson` is live compatibility — not dead code because HEAD's `currentJson` is higher. Check git history of `currentJson` before claiming old patches already used today's version. Do not replace those version branches with a JSON-type check.
- `plugin.json` must be UTF-8 without BOM (file starts with `{`). Prefer `StrReplace`. Do not require `scripts/` (not in this tree).
- C++: tabs in `src/`, VCV-style names, no C++14/17-only APIs, no blank line between every statement.
- When assigning `inputInfos` / `outputInfos` names later, prepend `monoPortPrefix()` / `polyPortPrefix()`.
