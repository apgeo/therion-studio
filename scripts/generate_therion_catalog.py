#!/usr/bin/env python3
"""Generate Therion command catalog JSON from thbook TeX sources.

Current parser scope:
- therion/thbook/ch02.tex
- therion/thbook/ch03.tex

The parser is intentionally conservative: it extracts section-level command
metadata and preserves textual details for downstream tooling.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import re
from pathlib import Path
from typing import Any


SECTION_RE = re.compile(r"^\\subsubchapter\s+`([^'\n]+)'\.?\s*$", re.MULTILINE)
PIPE_TOKEN_RE = re.compile(r"\|([^|]+)\|")
NEW_TAG_RE = re.compile(r"\\NEW\{[^}]*\}")
BRACKET_NOTE_RE = re.compile(r"\\\[[^\]]*\\\]")
TEX_CMD_RE = re.compile(r"\\[a-zA-Z]+\*?(?:\{[^}]*\})?")
CONTEXT_ORDER = [
    "none",
    "all",
    "survey",
    "scrap",
    "centerline",
    "map",
    "surface",
    "layout",
    "lookup",
]
CENTERLINE_INLINE_COMMAND_NAMES = {
    "date",
    "explo-date",
    "team",
    "explo-team",
    "instrument",
    "infer",
    "declination",
    "grid-angle",
    "sd",
    "grade",
    "units",
    "calibrate",
    "break",
    "mark",
    "flags",
    "station",
    "cs",
    "fix",
    "equate",
    "data",
    "group",
    "endgroup",
    "walls",
    "vthreshold",
    "extend",
    "station-names",
}


def normalize_whitespace(value: str) -> str:
    return re.sub(r"\s+", " ", value).strip()


def append_unique(items: list[str], value: str) -> None:
    normalized = normalize_whitespace(value).lower()
    if not normalized or normalized in items:
        return
    items.append(normalized)


def symbol_keyword_from_token(token: str) -> str:
    normalized = normalize_whitespace(token).lower()
    if not re.fullmatch(r"[a-z][a-z0-9-]*", normalized):
        return ""
    return normalized


def extract_signature_core(signature: str) -> str:
    pipe_tokens = [normalize_whitespace(token) for token in PIPE_TOKEN_RE.findall(signature)]
    if pipe_tokens:
        return pipe_tokens[0]
    return normalize_whitespace(signature)


def extract_option_key(signature_core: str) -> str:
    if not signature_core:
        return ""
    first_token = signature_core.split()[0].strip().split(",")[0]
    if not first_token:
        return ""
    first_alias = first_token.split("/")[0].strip()
    if not first_alias:
        return ""
    normalized = first_alias.lower().replace("[", "").replace("]", "")
    if normalized.startswith("<") or normalized.startswith("["):
        return ""
    if normalized.startswith("-"):
        normalized = normalized.lstrip("-")

    if not re.fullmatch(r"[a-z][a-z0-9-]*", normalized):
        return ""
    return f"-{normalized}"


def infer_value_arity(signature_core: str, raw_signature: str) -> str:
    if not signature_core:
        return "0"

    core = signature_core
    parts = core.split(maxsplit=1)
    payload = parts[1] if len(parts) > 1 else ""
    payload = payload.strip()
    if not payload:
        return "0"

    if "..." in payload or "..." in raw_signature:
        return "N"

    placeholder_count = len(re.findall(r"<[^>]+>", payload))
    bracket_count = len(re.findall(r"\[[^\]]+\]", payload))
    token_count = len(payload.split())
    if placeholder_count + bracket_count > 1:
        return "N"
    if placeholder_count + bracket_count == 1:
        return "1"
    if token_count > 1:
        return "N"
    return "1"


def infer_value_domain(signature_core: str, raw_signature: str, allowed_values: list[str], value_arity: str) -> str:
    if value_arity == "0":
        return "none"

    if allowed_values:
        return "enum"

    text = f"{signature_core} {raw_signature}".lower()
    if "<on/off>" in text or "on/off" in text:
        return "enum"
    if "<number>" in text or "<n>" in text or "<num>" in text or "<value>" in text:
        return "number"
    if "<date>" in text or "date" in text:
        return "date"
    if "<encoding" in text or "encoding" in text:
        return "encoding"
    if "<file" in text or "file-name" in text or "filename" in text or "directory" in text or "path" in text:
        return "path"
    if "<string>" in text or "string" in text or "text" in text:
        return "string"
    if "station" in text:
        return "station"
    if "keyword" in text:
        return "keyword"
    if "projection" in text:
        return "projection"
    return "unknown"


def infer_allowed_values(signature_core: str, raw_signature: str) -> list[str]:
    allowed_values: list[str] = []
    text = f"{signature_core} {raw_signature}"

    # Enumerations commonly appear as <a/b/c>.
    for match in re.findall(r"<([^>]+)>", text):
        candidate = normalize_whitespace(match)
        if "/" not in candidate:
            continue
        parts = [part.strip() for part in candidate.split("/") if part.strip()]
        for part in parts:
            if not re.fullmatch(r"[A-Za-z0-9_.:-]+", part):
                continue
            if part not in allowed_values:
                allowed_values.append(part)

    # Some options use syntax like (none)/horizontal/vertical.
    for match in re.findall(r"\(([^)]+)\)((?:/[A-Za-z0-9_.:-]+)+)", text):
        first_token = match[0].strip()
        tail_tokens = [token.strip() for token in match[1].split("/") if token.strip()]
        candidates = [first_token, *tail_tokens]
        for part in candidates:
            if not re.fullmatch(r"[A-Za-z0-9_.:-]+", part):
                continue
            if part not in allowed_values:
                allowed_values.append(part)

    if "on" in allowed_values and "off" in allowed_values:
        # Keep deterministic order for common boolean enum.
        allowed_values = [value for value in allowed_values if value not in {"on", "off"}]
        allowed_values = ["on", "off", *allowed_values]

    return allowed_values


def clean_tex_text(value: str) -> str:
    stripped = value.replace("\r\n", "\n")
    stripped = NEW_TAG_RE.sub("", stripped)
    # Strip TeX comments while preserving escaped percent signs.
    stripped = re.sub(r"(?<!\\)%[^\n]*", "", stripped)
    # Convert \[...\] and \[...] note wrappers to plain text content.
    stripped = re.sub(r"\\\[\s*([^\]]*?)\s*\\\]", r" (\1) ", stripped, flags=re.DOTALL)
    stripped = re.sub(r"\\\[\s*([^\]]*?)\s*\]", r" (\1) ", stripped, flags=re.DOTALL)
    stripped = BRACKET_NOTE_RE.sub("", stripped)
    stripped = stripped.replace("\\hfill\\break", "\n")
    stripped = stripped.replace("\\qquad", " ")
    stripped = stripped.replace("~", " ")
    stripped = stripped.replace("---", " - ")
    stripped = stripped.replace("--", " - ")
    stripped = TEX_CMD_RE.sub(" ", stripped)
    stripped = stripped.replace("{", " ").replace("}", " ")
    return normalize_whitespace(stripped)


def parse_item_block(block_text: str) -> list[dict[str, Any]]:
    items: list[str] = []
    current: list[str] = []
    for raw_line in block_text.splitlines():
        line = raw_line.strip()
        if not line:
            continue
        if line.startswith("*"):
            if current:
                items.append(" ".join(current))
            current = [line[1:].strip()]
        else:
            if current:
                current.append(line)
    if current:
        items.append(" ".join(current))

    parsed_items: list[dict[str, Any]] = []
    for raw_item in items:
        item = normalize_whitespace(raw_item)
        lhs, rhs = (item.split("=", 1) + [""])[:2] if "=" in item else (item, "")
        lhs = normalize_whitespace(lhs)
        rhs = normalize_whitespace(rhs)

        signature_core = extract_signature_core(lhs)
        name = signature_core if signature_core else lhs
        allowed_values = infer_allowed_values(signature_core, lhs)

        dependencies = []
        lowered_rhs = rhs.lower()
        if "valid only" in lowered_rhs or "only when" in lowered_rhs or "inside" in lowered_rhs:
            dependencies.append(rhs)

        option_key = extract_option_key(signature_core)
        value_arity = infer_value_arity(signature_core, lhs)
        value_domain = infer_value_domain(signature_core, lhs, allowed_values, value_arity)

        parsed_items.append(
            {
                "raw": item,
                "name": name,
                "signature": lhs,
                "option_key": option_key,
                "value_arity": value_arity,
                "value_domain": value_domain,
                "description": rhs,
                "allowed_values": allowed_values,
                "dependencies": dependencies,
            }
        )
    return parsed_items


def extract_block(section_text: str, tag: str) -> list[str]:
    pattern = re.compile(rf"\\{tag}\b(.*?)(?:\\end{tag})", re.DOTALL)
    return [match.group(1).strip() for match in pattern.finditer(section_text)]


def extract_contexts(section_text: str) -> list[str]:
    contexts: list[str] = []
    for block in extract_block(section_text, "context"):
        if not block:
            continue

        normalized_block = clean_tex_text(block).lower()
        if not normalized_block:
            continue

        if "very first command" in normalized_block or "first command in the file" in normalized_block:
            contexts.append("none")
        if re.search(r"\bnone\b", normalized_block):
            contexts.append("none")

        if re.search(r"\ball\b", normalized_block):
            contexts.append("all")
        if re.search(r"\bsurvey\b", normalized_block):
            contexts.append("survey")
        if re.search(r"\bscrap\b", normalized_block):
            contexts.append("scrap")
        if re.search(r"\bcentreline\b|\bcenterline\b", normalized_block):
            contexts.append("centerline")
        if re.search(r"\bmap\b", normalized_block):
            contexts.append("map")
        if re.search(r"\bsurface\b", normalized_block):
            contexts.append("surface")
        if re.search(r"\blayout\b", normalized_block):
            contexts.append("layout")
        if re.search(r"\blookup\b", normalized_block):
            contexts.append("lookup")

    deduped: list[str] = []
    for context_name in CONTEXT_ORDER:
        if context_name in contexts:
            deduped.append(context_name)
    return deduped


def extract_section_description(section_text: str) -> str:
    description_blocks = extract_block(section_text, "description")
    if description_blocks:
        return clean_tex_text(description_blocks[0])

    first_macro = re.search(r"\\(syntax|arguments|options|context)\b", section_text)
    prose = section_text[: first_macro.start()] if first_macro else section_text
    prose = re.sub(r"\\subsubchapter\s+`[^`]+'\.?", "", prose)
    return clean_tex_text(prose)


def extract_symbol_type_values(arguments: list[dict[str, Any]]) -> list[str]:
    type_values: list[str] = []
    type_argument = next((item for item in arguments if item.get("name", "").strip().lower() == "<type>"), None)
    if type_argument is None:
        return type_values

    type_text = f"{type_argument.get('signature', '')} {type_argument.get('description', '')}"
    type_text = NEW_TAG_RE.sub("", type_text)
    type_text = re.sub(r"(?<!\\)%[^\n]*", "", type_text)
    type_text = re.sub(r"\\\[\s*([^\]]*?)\s*\\\]", " ", type_text, flags=re.DOTALL)
    type_text = re.sub(r"\\\[\s*([^\]]*?)\s*\]", " ", type_text, flags=re.DOTALL)
    type_text = BRACKET_NOTE_RE.sub(" ", type_text)
    for token in PIPE_TOKEN_RE.findall(type_text):
        keyword = symbol_keyword_from_token(token)
        if not keyword:
            continue
        append_unique(type_values, keyword)
    return type_values


def extract_symbol_subtype_matrix(section_text: str) -> dict[str, list[str]]:
    subtype_matrix: dict[str, list[str]] = {}
    subtype_anchor = re.search(r"\|\s*subtype\s*<restr_keyword>\s*\|", section_text, re.IGNORECASE)
    if subtype_anchor is None:
        return subtype_matrix

    tail = section_text[subtype_anchor.start():]
    italic_groups = re.finditer(r"\{\\it\s+([^:}]+):\}\s*(.*?);", tail, re.DOTALL)
    for group in italic_groups:
        type_name = symbol_keyword_from_token(group.group(1))
        if not type_name:
            continue

        subtype_values: list[str] = subtype_matrix.get(type_name, [])
        for token in PIPE_TOKEN_RE.findall(group.group(2)):
            keyword = symbol_keyword_from_token(token)
            if not keyword:
                continue
            append_unique(subtype_values, keyword)
        if subtype_values:
            subtype_matrix[type_name] = subtype_values

    return subtype_matrix


def extract_centerline_data_value_groups(description: str) -> tuple[list[str], list[str]]:
    style_values: list[str] = []
    reading_values: list[str] = []
    if not description:
        return style_values, reading_values

    style_match = re.search(r"style\s*\(([^)]+)\)", description, re.IGNORECASE)
    if style_match:
        for token in style_match.group(1).split(","):
            keyword = symbol_keyword_from_token(token)
            if keyword:
                append_unique(style_values, keyword)

    readings_match = re.search(
        r"keywords:\s*(.*?)(?:\.\s*See\s+Survex\s+manual|\.\s*For\s+interleaved\s+data|$)",
        description,
        re.IGNORECASE | re.DOTALL,
    )
    if not readings_match:
        return style_values, reading_values

    readings_text = readings_match.group(1)

    # Expand [back]token forms into both forward and backward variants
    # (e.g. [back]tape -> tape, backtape) before generic token splitting.
    back_token_pattern = re.compile(r"\[back\]\s*([a-z][a-z0-9-]*)", re.IGNORECASE)
    for match in back_token_pattern.finditer(readings_text):
        base = symbol_keyword_from_token(match.group(1))
        if not base:
            continue
        append_unique(reading_values, base)
        append_unique(reading_values, f"back{base}")

    readings_text = re.sub(r"\\\[\s*dimension.*?shot\]", " ", readings_text, flags=re.IGNORECASE | re.DOTALL)
    readings_text = re.sub(r"\\[a-zA-Z]+(?:\d+)?(?:\{[^}]*\})?", " ", readings_text)
    readings_text = readings_text.replace("[back]", "back")
    readings_text = readings_text.replace("/", ",")
    readings_text = readings_text.replace("\\", " ")

    for token in readings_text.split(","):
        normalized = normalize_whitespace(token).lower()
        if not normalized:
            continue
        normalized = normalized.strip("`'\"|.:-[]()")
        normalized = re.sub(r"^\d+", "", normalized)
        keyword = symbol_keyword_from_token(normalized)
        if keyword:
            append_unique(reading_values, keyword)

    return style_values, reading_values


def extract_centerline_data_allowed_values(description: str) -> list[str]:
    style_values, reading_values = extract_centerline_data_value_groups(description)
    allowed_values: list[str] = []
    for keyword in style_values:
        append_unique(allowed_values, keyword)
    for keyword in reading_values:
        append_unique(allowed_values, keyword)
    return allowed_values


def extract_supported_flag_keywords(description: str, skip: set[str]) -> list[str]:
    keywords: list[str] = []
    for match in re.finditer(r"supported flags?(?: are)?:\s*([^.]*)", description, re.IGNORECASE):
        clause = match.group(1)
        for token in PIPE_TOKEN_RE.findall(clause):
            keyword = symbol_keyword_from_token(token)
            if not keyword or keyword in skip:
                continue
            append_unique(keywords, keyword)
    return keywords


def extract_one_of_keywords_from_description(description: str, skip: set[str]) -> list[str]:
    keywords: list[str] = []
    match = re.search(r"is one of:\s*([^.]*)", description, re.IGNORECASE)
    if not match:
        return keywords

    candidate = match.group(1).replace(" and ", ",")
    for token in candidate.split(","):
        normalized = normalize_whitespace(re.sub(r"\([^)]*\)", "", token)).lower()
        keyword = symbol_keyword_from_token(normalized)
        if not keyword or keyword in skip:
            continue
        append_unique(keywords, keyword)
    return keywords


def extract_centerline_inline_allowed_values(inline_command_name: str, description: str, seed_values: list[str]) -> list[str]:
    allowed_values: list[str] = []
    for token in seed_values:
        append_unique(allowed_values, token)

    if inline_command_name == "data":
        style_values, reading_values = extract_centerline_data_value_groups(description)
        for token in style_values:
            append_unique(allowed_values, token)
        for token in reading_values:
            append_unique(allowed_values, token)
        return allowed_values

    skip = {inline_command_name, *CENTERLINE_INLINE_COMMAND_NAMES}
    for token in extract_one_of_keywords_from_description(description, skip):
        append_unique(allowed_values, token)
    for token in extract_supported_flag_keywords(description, skip):
        append_unique(allowed_values, token)
    return allowed_values


def build_argument_entries_from_signature(signature_core: str) -> list[dict[str, Any]]:
    arguments: list[dict[str, Any]] = []
    if not signature_core:
        return arguments

    parts = signature_core.split(maxsplit=1)
    payload = parts[1] if len(parts) > 1 else ""
    payload = payload.strip()
    if not payload:
        return arguments

    for match in re.finditer(r"\[[^\]]+\]|<[^>]+>", payload):
        signature = normalize_whitespace(match.group(0))
        if not signature:
            continue
        arguments.append(
            {
                "raw": signature,
                "name": signature,
                "signature": signature,
                "option_key": "",
                "value_arity": "1" if signature.startswith("<") else "N",
                "value_domain": "keyword",
                "description": "",
                "allowed_values": [],
                "dependencies": [],
            }
        )
    return arguments


def parse_sections(tex_text: str, source_file: str, known_command_names: set[str] | None = None) -> list[dict[str, Any]]:
    matches = list(SECTION_RE.finditer(tex_text))
    results = []
    for index, match in enumerate(matches):
        command_name = normalize_whitespace(match.group(1))
        if "xtherion" in command_name.lower():
            continue

        start = match.start()
        end = matches[index + 1].start() if index + 1 < len(matches) else len(tex_text)
        section_text = tex_text[start:end]

        syntax_blocks = [clean_tex_text(block) for block in extract_block(section_text, "syntax")]
        argument_blocks = extract_block(section_text, "arguments")
        option_blocks = extract_block(section_text, "options")
        comopt_blocks = extract_block(section_text, "comopt")
        contexts = extract_contexts(section_text)
        description = extract_section_description(section_text)

        arguments = []
        for block in argument_blocks:
            arguments.extend(parse_item_block(block))
        options = []
        for block in option_blocks:
            options.extend(parse_item_block(block))
        comopt_items: list[dict[str, Any]] = []
        for block in comopt_blocks:
            comopt_items.extend(parse_item_block(block))

        normalized_command_name = command_name.strip().lower()
        if normalized_command_name not in {"centreline", "centerline"}:
            options.extend(comopt_items)
        deduped_options = []
        seen_option_signatures: set[str] = set()
        for option in options:
            signature = normalize_whitespace(option.get("signature", "")).lower()
            if not signature or signature in seen_option_signatures:
                continue
            seen_option_signatures.add(signature)
            deduped_options.append(option)
        options = deduped_options

        dependencies = []
        if contexts:
            dependencies.append(f"context: {', '.join(contexts)}")
        for opt in options:
            for dep in opt.get("dependencies", []):
                if dep not in dependencies:
                    dependencies.append(dep)

        # Command-level allowed values must describe command argument domains.
        # Option-level enum values stay attached to each option entry and must not
        # be merged into the parent command, otherwise identifier arguments (e.g.
        # survey <id>) get polluted with unrelated option enums like on/off.
        allowed_values: list[str] = []

        result_entry = {
            "name": command_name,
            "source": {
                "file": source_file,
                "line": tex_text.count("\n", 0, start) + 1,
            },
            "summary": description,
            "syntax": syntax_blocks,
            "contexts": contexts,
            "arguments": arguments,
            "options": options,
            "dependencies": dependencies,
            "allowed_values": allowed_values,
        }

        if normalized_command_name in {"centreline", "centerline"}:
            inline_commands: list[str] = []
            for item in comopt_items:
                option_key = normalize_whitespace(item.get("option_key", ""))
                if not option_key.startswith("-"):
                    continue
                append_unique(inline_commands, option_key[1:])
            if inline_commands:
                result_entry["inline_commands"] = inline_commands

            for item in comopt_items:
                option_key = normalize_whitespace(item.get("option_key", ""))
                if not option_key.startswith("-"):
                    continue

                inline_command_name = option_key[1:].strip().lower()
                if not inline_command_name:
                    continue
                if known_command_names and inline_command_name in known_command_names and inline_command_name != "data":
                    continue

                signature = normalize_whitespace(item.get("signature", ""))
                signature_core = extract_signature_core(signature)
                arguments_for_command = build_argument_entries_from_signature(signature_core)
                description_for_command = clean_tex_text(item.get("description", ""))
                if inline_command_name == "data":
                    style_values, reading_values = extract_centerline_data_value_groups(item.get("description", ""))
                    if arguments_for_command:
                        arguments_for_command[0]["allowed_values"] = style_values
                        arguments_for_command[0]["value_domain"] = "enum" if style_values else "keyword"
                        arguments_for_command[0]["value_arity"] = "1"
                    if len(arguments_for_command) >= 2:
                        arguments_for_command[1]["allowed_values"] = reading_values
                        arguments_for_command[1]["value_domain"] = "enum" if reading_values else "keyword"
                        arguments_for_command[1]["value_arity"] = "N"
                allowed_for_command = extract_centerline_inline_allowed_values(
                    inline_command_name,
                    item.get("description", ""),
                    list(item.get("allowed_values", [])),
                )

                command_line = tex_text.count("\n", 0, start) + 1
                signature_anchor = section_text.lower().find(signature.lower())
                if signature and signature_anchor >= 0:
                    command_line = tex_text.count("\n", 0, start + signature_anchor) + 1

                results.append(
                    {
                        "name": inline_command_name,
                        "source": {
                            "file": source_file,
                            "line": command_line,
                        },
                        "summary": description_for_command,
                        "syntax": [signature] if signature else [],
                        "contexts": ["centerline"],
                        "arguments": arguments_for_command,
                        "options": [],
                        "dependencies": ["context: centerline"],
                        "allowed_values": allowed_for_command,
                    }
                )

        if normalized_command_name in {"point", "line", "area"}:
            type_values = extract_symbol_type_values(arguments)
            if type_values:
                option_names = {
                    option_key[1:]
                    for option_key in (option.get("option_key", "") for option in options)
                    if isinstance(option_key, str) and option_key.startswith("-")
                }
                type_values = [value for value in type_values if value not in option_names]
                result_entry["type_values"] = type_values

            subtype_matrix = extract_symbol_subtype_matrix(section_text)
            if normalized_command_name == "area":
                type_argument = next((item for item in arguments if item.get("name", "").strip().lower() == "<type>"), None)
                type_description = (
                    f"{type_argument.get('signature', '')} {type_argument.get('description', '')}".lower()
                    if type_argument
                    else ""
                )
                if "arbitrary subtype" in type_description:
                    subtype_matrix.setdefault("u", ["*"])

            if subtype_matrix:
                result_entry["subtype_by_type"] = subtype_matrix

        results.append(result_entry)
    return results


def apply_overrides(catalog: dict[str, Any], overrides: dict[str, Any]) -> dict[str, Any]:
    commands_by_name = {command["name"]: command for command in catalog["commands"]}
    for command_name, patch in overrides.get("command_patches", {}).items():
        command = commands_by_name.get(command_name)
        if command is None:
            continue
        for key, value in patch.items():
            command[key] = value

    for extra in overrides.get("extra_commands", []):
        if not isinstance(extra, dict) or "name" not in extra:
            continue
        if extra["name"] in commands_by_name:
            continue
        catalog["commands"].append(extra)

    catalog["commands"] = sorted(catalog["commands"], key=lambda item: item["name"].lower())
    return catalog


def build_catalog(inputs: list[Path], source_repo: str, source_ref: str) -> dict[str, Any]:
    commands = []
    source_texts: list[tuple[Path, str]] = []
    known_command_names: set[str] = set()
    for input_path in inputs:
        text = input_path.read_text(encoding="utf-8", errors="replace")
        source_texts.append((input_path, text))
        for match in SECTION_RE.finditer(text):
            command_name = normalize_whitespace(match.group(1)).lower()
            if "xtherion" in command_name:
                continue
            known_command_names.add(command_name)

    for input_path, text in source_texts:
        commands.extend(parse_sections(text, str(input_path), known_command_names))

    # Deduplicate by command name while preserving first occurrence precedence.
    deduped = {}
    for command in commands:
        deduped.setdefault(command["name"], command)

    return {
        "metadata": {
            "generated_at_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
            "generator_version": "1",
            "source_repository": source_repo,
            "source_reference": source_ref,
            "source_files": [str(path) for path in inputs],
        },
        "commands": sorted(deduped.values(), key=lambda item: item["name"].lower()),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate Therion command metadata catalog from thbook TeX.")
    parser.add_argument(
        "--input",
        action="append",
        default=[],
        help="Input TeX file. Can be supplied multiple times.",
    )
    parser.add_argument(
        "--output",
        default="resources/therion_command_catalog.json",
        help="Output JSON path.",
    )
    parser.add_argument(
        "--overrides",
        default="resources/therion_command_catalog.overrides.json",
        help="Overrides JSON path.",
    )
    parser.add_argument(
        "--source-repo",
        default="https://github.com/therion/therion",
        help="Upstream source repository metadata.",
    )
    parser.add_argument(
        "--source-ref",
        default="snapshot",
        help="Upstream source reference metadata (tag/commit/etc.).",
    )
    args = parser.parse_args()

    input_paths = [Path(p) for p in args.input] if args.input else [
        Path("therion/thbook/ch02.tex"),
        Path("therion/thbook/ch03.tex"),
    ]

    for input_path in input_paths:
        if not input_path.exists():
            raise FileNotFoundError(f"Missing input file: {input_path}")

    catalog = build_catalog(input_paths, args.source_repo, args.source_ref)

    overrides_path = Path(args.overrides)
    if overrides_path.exists():
        overrides = json.loads(overrides_path.read_text(encoding="utf-8"))
        catalog = apply_overrides(catalog, overrides)

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(catalog, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Generated {output_path} with {len(catalog['commands'])} commands.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
