---
name: build-ci
description: Guides CMake presets, build outputs, formatting, and CI workflow in VeloZ. Invoke when editing build files, scripts, or CI configs.
---

# Build and CI

## Purpose

Use this skill when changing CMake, build presets, formatting, or CI pipelines.

## CMake Presets (Mandatory)
- Presets: dev (Debug), release (Release), asan (Clang + ASan/UBSan).
- Configure: cmake --preset <dev|release|asan>
- Build: cmake --build --preset <preset>-all -j$(nproc)
- Targeted builds: dev-engine, dev-libs, dev-tests (and release/asan equivalents).

## Build Outputs
- build/<preset>/apps/engine/veloz_engine
- build/<preset>/libs/veloz_*.a
- build/<preset>/libs/*/veloz_*_tests

## Formatting (Mandatory)
- Use ./scripts/format.sh for C++.
- Supports --check for CI verification.

## CI Pipeline (Mandatory)
- dev build: configure dev, build dev-all, ctest --preset dev, smoke run engine.
- release build: configure release, build release-all, smoke run engine.
- asan build: configure asan, build asan-all, run ctest in build/asan.
- python-check: flake8 on python/ and apps/gateway/.
- docs-check: ensure README.md, CLAUDE.md, docs/ exist.

## Build Scripts
- ./scripts/build.sh <preset> wraps preset configure + build.
- ./scripts/run_engine.sh and ./scripts/run_gateway.sh build and run.

## KJ Build Integration
- KJ is built from libs/core/kj via add_subdirectory(kj).
- Link against kj and kj-async in core targets.
