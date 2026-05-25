# Therion Catalog Layout

This directory contains generated per-command metadata and local override inputs.

- `../therion_command_catalog.json`: combined catalog consumed by the app/runtime.
- `commands/*.json`: one generated file per command (for review and targeted diffs).
- `overrides/*.override.json`: optional per-command override patches applied during generation.

## Override naming

- Override file name must match command name:
  - `overrides/centreline.override.json`
  - `overrides/encoding.override.json`

## Override payload shape

Each override file is a JSON object. Supported keys:

- `patch`: deep-merge patch for command-level fields.
- `options_by_key`: patches specific command options by `option_key`.
- `arguments_by_name`: patches arguments by `name`.
- `arguments_by_index`: patches arguments by 0-based index.

Minimal example:

```json
{
  "patch": {
    "aliases": ["centerline"]
  }
}
```

## Regeneration

Run:

```bash
scripts/refresh_therion_catalog.sh --skip-update
```

This rewrites both:

- `resources/therion_command_catalog.json`
- `resources/therion_catalog/commands/*.json`
