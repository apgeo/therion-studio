# Therion Command Catalog Pipeline

This repository keeps a generated Therion command catalog in:

- `resources/therion_command_catalog.json`

The catalog is generated from a pinned snapshot of Therion `thbook` sources:

- `therion/thbook/ch02.tex`
- `therion/thbook/ch03.tex`
- `therion/thbook/thbook.tex`

Source snapshot metadata is tracked in:

- `docs/therion-source.lock`

## Why this pipeline exists

- Keep completion/help/configuration metadata data-driven.
- Avoid hand-maintaining command/option/value lists in UI code.
- Make updates reproducible when upstream Therion docs change.

## Scripts

- `scripts/update_thbook_snapshot.sh`
  - Pulls `thbook` files from a specific upstream repo + ref (tag/commit).
  - Rewrites `docs/therion-source.lock` including SHA-256 hashes.

- `scripts/generate_therion_catalog.py`
  - Parses `ch02.tex` and `ch03.tex`.
  - Produces `resources/therion_command_catalog.json`.
  - Merges local overrides from `resources/therion_command_catalog.overrides.json`.

- `scripts/refresh_therion_catalog.sh`
  - End-to-end helper: optional snapshot update + catalog regeneration.

- `scripts/check_therion_catalog.sh`
  - CI/local check: fails if generated output differs from committed catalog.

## Recommended update workflow

1. Update snapshot from upstream Therion:

```bash
scripts/update_thbook_snapshot.sh \
  --repo https://github.com/therion/therion.git \
  --ref <tag-or-commit>
```

2. Regenerate catalog:

```bash
scripts/refresh_therion_catalog.sh --skip-update
```

3. Validate generated output:

```bash
scripts/check_therion_catalog.sh
```

4. Review and commit together:
- `therion/thbook/ch02.tex`
- `therion/thbook/ch03.tex`
- `therion/thbook/thbook.tex`
- `docs/therion-source.lock`
- `resources/therion_command_catalog.json`
- `resources/therion_command_catalog.overrides.json` (if changed)

## Override policy

Use `resources/therion_command_catalog.overrides.json` only for:

- alias normalization (for example `centreline` / `centerline`)
- missing data that cannot be extracted reliably from TeX
- explicit compatibility patches

Parser-level extraction is the default and required path for Therion metadata behavior changes; do not add persistent overrides for data that can be parsed from source.

Do not hardcode Therion command semantics in UI code when it can be represented in catalog JSON or overrides.

## Option-shape fields (current generator output)

Each parsed option entry includes normalized shape hints for UI/autocomplete:

- `option_key`: normalized key (`-id`, `-recursive`, `-d`, ...)
- `value_arity`: `0`, `1`, or `N`
- `value_domain`: coarse domain hint (`enum`, `number`, `string`, `path`, `keyword`, ...)

These fields are heuristic and should be improved by parser refinements and regression tests rather than edited manually in generated JSON.

## Centerline inline commands

`centreline`/`centerline` includes a dedicated structured field:

- `inline_commands`: inline centerline subcommands parsed from `\comopt` blocks (for example `data`, `date`, `team`, `units`, `station`, `extend`)

This field is intentionally separate from `options` to avoid mixing real dash-options with argument placeholders or prose tokens.

Generator also emits standalone centerline-context command entries for inline commands that do not have dedicated top-level sections, so autocomplete/help can use per-inline-command value metadata (for example `infer` -> `on/off`, `walls` -> `auto/on/off`, `mark` -> `fixed/painted/temporary`).

## Future-proofing guidance

- Prefer upstream tags for `--ref` (stable releases) over floating branches.
- Keep lock file as the source-of-truth for provenance and reproducibility.
- If TeX structure changes upstream, fix parser extraction in `generate_therion_catalog.py` and regenerate; avoid manual edits to generated JSON.
- Run `scripts/check_therion_catalog.sh` in CI to prevent stale catalog drift.
