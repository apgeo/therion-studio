# Encoding QA Checklist

Last updated: 2026-05-14

Use this checklist for manual verification of encoding workflows across macOS, Windows, and Linux.

## 1. Test Matrix

Run all scenarios below on each platform:

- macOS
- Windows
- Linux

Run with sample files that contain non-ASCII characters (for example Czech diacritics) and these directives:

- `encoding utf-8`
- `encoding iso-8859-1`
- `encoding windows-1250`
- `encoding cp1250` (alias path)
- `encoding windows-1252`

## 2. Open and Detect

For each sample file:

1. Open project and file in text editor.
2. Confirm status row shows expected encoding label.
3. Confirm `Convert to UTF-8` action visibility:
   - hidden for UTF-8
   - visible for non-UTF encodings
4. Confirm special characters are readable (not mojibake).

Expected result:

- File decodes with the declared encoding when supported by Qt.
- Encoding label matches detected/declared codec semantics used by the app.

## 3. Save Without Conversion

For each non-UTF sample:

1. Make a harmless text edit (for example add/remove a comment space).
2. Save file without conversion.
3. Reopen file.
4. Confirm status row still shows original non-UTF encoding.
5. Confirm characters remain correct.

Expected result:

- Save preserves original encoding bytes class (no forced UTF-8 conversion).

## 4. Convert to UTF-8

For each non-UTF sample:

1. Click `Convert to UTF-8`.
2. Confirm conversion dialog appears.
3. Accept conversion.
4. Confirm tab becomes dirty.
5. Save file.
6. Reopen file.
7. Confirm status row reports UTF-8.
8. Confirm characters remain correct.

Expected result:

- Conversion is explicit and only persisted after save.

## 5. Byte-Level Validation

Do byte checks for at least one file per encoding family:

1. Keep a pre-save copy.
2. After save-without-conversion, compare bytes against expectation for that codec (changed content only where edited).
3. After UTF-8 conversion save, verify file bytes are valid UTF-8.

Recommended tools:

- macOS/Linux: `xxd`, `file -I`, `iconv` checks
- Windows: `certutil -dump`, editor/hex viewer with codepage support

## 6. Regression Guardrails

Also validate:

- No crash when opening/saving non-UTF files.
- No unexpected encoding label flips after reopen.
- `encoding cp1250` alias resolves correctly through normalization path.
- Large-file save path remains responsive.

## 7. Test Log Template

Record each run with:

- Date:
- Platform:
- Sample file:
- Declared encoding:
- Detected encoding label:
- Save-without-conversion: Pass/Fail
- Convert-to-UTF-8: Pass/Fail
- Byte check: Pass/Fail
- Notes/issues:
