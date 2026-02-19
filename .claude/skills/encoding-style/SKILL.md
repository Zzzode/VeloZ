---
name: encoding-style
description: Defines encoding, language, naming, and formatting conventions for VeloZ. Invoke when writing or editing code, comments, or docs.
---

# Encoding and Style

## Purpose

Use this skill to ensure code, comments, and documentation follow repository conventions.

## Language and Encoding

- English only across code, comments, and documentation.
- UTF-8 encoding, no BOM, LF line endings.

## Naming Conventions

### C++

- Files: snake_case.cpp / snake_case.h
- Classes/Structs: TitleCase
- Functions/Methods: snake_case
- Variables: snake_case
- Constants/Macros: CAPITAL_WITH_UNDERSCORES
- Private members: snake_case_
- Namespaces: lowercase

### Python

- Files/Modules: snake_case.py
- Classes: TitleCase
- Functions/Variables: snake_case
- Constants: UPPER_CASE

## C++ Formatting

- 2 spaces indentation, no tabs.
- Max line length 100.
- Opening brace on same line, closing brace on new line.
- Include order: project headers, std headers, KJ headers, then third-party headers.
- Use // comments only, with English text.

## Python Formatting

- PEP 8 conventions.
- 4 spaces indentation.
- Line length 120.
- Import order: stdlib, third-party, local.

## JSON and Event Formatting

- Engine stdout uses NDJSON, one JSON object per line.
- Field names are snake_case.
- Timestamps use ISO 8601 UTC.
