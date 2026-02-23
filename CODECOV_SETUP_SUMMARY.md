# Codecov Setup Summary

This document summarizes the Codecov integration for VeloZ.

## What Was Added

### 1. CMake Configuration

**File**: `CMakePresets.json`

Added a new `coverage` preset for building with coverage instrumentation:

- **Configure preset**: `coverage` (GCC Debug with coverage flags)
- **Build presets**: `coverage-all`, `coverage-tests`
- **Test preset**: `coverage`

Coverage flags:
- Compiler: GCC (better coverage support)
- CXX flags: `-fprofile-arcs -ftest-coverage -O0 -g`
- Linker flags: `-fprofile-arcs -ftest-coverage`

### 2. CI Workflow

**File**: `.github/workflows/ci.yml`

Added a new `coverage` job that:
1. Installs lcov for coverage collection
2. Configures with the `coverage` preset
3. Builds all targets with coverage instrumentation
4. Runs all tests
5. Generates coverage report with lcov
6. Uploads to Codecov using `codecov/codecov-action@v4`

### 3. Codecov Configuration

**File**: `codecov.yml`

Configured coverage reporting:
- Target: Auto (maintains current coverage)
- Threshold: 1% (allows minor decreases)
- Range: 70-100%
- Ignored paths: tests, gateway, UI, test files

### 4. Local Coverage Script

**File**: `scripts/coverage.sh`

Created a convenience script for local coverage testing:
- Configures and builds with coverage
- Runs all tests
- Generates lcov report
- Creates HTML report in `coverage_html/`

### 5. Documentation

**File**: `docs/guides/codecov-setup.md`

Comprehensive guide covering:
- Initial Codecov setup
- GitHub secrets configuration
- CI integration details
- Local coverage testing
- Troubleshooting tips

### 6. Updated Files

- **README.md**: Added CI and Codecov badges
- **CLAUDE.md**: Added coverage preset documentation
- **.gitignore**: Added coverage artifacts (*.gcda, *.gcno, coverage.info, coverage_html/)

## Setup Steps Required

### 1. Connect to Codecov

1. Go to https://codecov.io
2. Sign in with GitHub
3. Add the VeloZ repository
4. Copy the `CODECOV_TOKEN`

### 2. Add GitHub Secret

1. Go to GitHub repository → Settings → Secrets and variables → Actions
2. Create new secret: `CODECOV_TOKEN`
3. Paste the token from Codecov

### 3. Update README Badges

Replace `YOUR_USERNAME` in README.md with your actual GitHub username:

```markdown
[![CI](https://github.com/YOUR_USERNAME/VeloZ/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_USERNAME/VeloZ/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/YOUR_USERNAME/VeloZ/branch/master/graph/badge.svg)](https://codecov.io/gh/YOUR_USERNAME/VeloZ)
```

### 4. Push Changes

```bash
git add .
git commit -m "feat: Add Codecov integration for code coverage tracking"
git push
```

The coverage job will run automatically on the next CI run.

## Usage

### View Coverage in CI

After pushing, the CI workflow will:
1. Run the coverage job
2. Upload results to Codecov
3. Add coverage status to the commit
4. Comment on PRs with coverage changes

### View Coverage Locally

```bash
./scripts/coverage.sh
open coverage_html/index.html
```

### View Coverage on Codecov

Go to: `https://codecov.io/gh/YOUR_USERNAME/VeloZ`

## Coverage Scope

Coverage is collected for:
- ✅ All C++ source files in `libs/`
- ✅ All C++ source files in `apps/engine/`

Coverage excludes:
- ❌ Test files (`*_test.cpp`, `*_tests.cpp`)
- ❌ Python gateway code (`apps/gateway/`)
- ❌ UI code (`apps/ui/`)
- ❌ System headers (`/usr/*`)

## Benefits

1. **Visibility**: Coverage badges in README show coverage at a glance
2. **CI Integration**: Automatic coverage collection on every push/PR
3. **PR Comments**: See coverage impact of changes before merging
4. **Trends**: Track coverage over time on Codecov dashboard
5. **Local Testing**: Easy local coverage generation with `./scripts/coverage.sh`

## Next Steps

1. Complete the setup steps above
2. Push the changes to trigger the first coverage run
3. Monitor coverage on Codecov dashboard
4. Add coverage requirements to PR review process
5. Aim to maintain or improve coverage with each change

## Troubleshooting

If coverage is not uploading:
1. Check that `CODECOV_TOKEN` is set correctly in GitHub secrets
2. Verify the token matches the one in Codecov dashboard
3. Check CI logs for upload errors
4. Ensure tests are actually running and generating `.gcda` files

For more details, see `docs/guides/codecov-setup.md`.
