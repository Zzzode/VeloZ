---
name: ui
description: Guides the static UI’s gateway integration and SSE event alignment. Invoke when modifying apps/ui or API surface for UI.
---

# UI (Static)

## Purpose

Use this skill when changing the UI or its API integration.

## Current UI Model
- apps/ui/index.html is static, no Node/Vite build.
- Uses REST JSON endpoints and SSE at GET /api/stream.

## API Alignment
- SSE event names match engine/gateway event type values.
- Keep event type names consistent across layers.

## Default Gateway Address
- http://127.0.0.1:8080/
- If using file:// to open the HTML, set the explicit API base.
