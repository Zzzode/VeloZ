# Codecov Setup Guide

This guide explains how to set up and use Codecov for VeloZ code coverage reporting.

## Overview

VeloZ uses Codecov to track and visualize code coverage across the C++ codebase. Coverage is automatically collected during CI runs and uploaded to Codecov for analysis.

## Prerequisites

1. A GitHub account with access to the VeloZ repository
2. A Codecov account (sign up at https://codecov.io)
3. The repository connected to Codecov

## Initial Setup

### 1. Connect Repository to Codecov

1. Go to https://codecov.io and sign in with your GitHub account
2. Click "Add a repository" and select the VeloZ repository
3. Codecov will generate a `CODECOV_TOKEN` for your repository

### 2. Add Codecov Token to GitHub Secrets

1. Go to your GitHub repository settings
2. Navigate to "Secrets and variables" → "Actions"
3. Click "New repository secret"
4. Name: `CODECOV_TOKEN`
5. Value: Paste the token from Codecov
6. Click "Add secret"

### 3. Update README Badges

Replace `YOUR_USERNAME` in the README.md badges with your actual GitHub username:

```markdown
[![CI](https://github.com/YOUR_USERNAME/VeloZ/actions/workflows/ci.yml/badge.svg)](https://github.com/YOUR_USERNAME/VeloZ/actions/workflows/ci.yml)
[![codecov](https://codecov.io/gh/YOUR_USERNAME/VeloZ/branch/master/graph/badge.svg)](https://codecov.io/gh/YOUR_USERNAME/VeloZ)
```

## CI Integration

The CI workflow automatically:

1. Builds the project with coverage instrumentation (using the `coverage` preset)
2. Runs all tests
3. Collects coverage data using `lcov`
4. Uploads the coverage report to Codecov

Coverage is collected for:
- All C++ source files in `libs/` and `apps/engine/`
- Excluding test files, gateway Python code, and UI code

## Local Coverage Testing

To generate coverage reports locally:

```bash
# Run the coverage script
./scripts/coverage.sh
```

This will:
1. Configure and build with coverage instrumentation
2. Run all tests
3. Generate coverage data
4. Create an HTML report in `coverage_html/`

Open `coverage_html/index.html` in your browser to view the detailed coverage report.

## Coverage Configuration

Coverage settings are defined in `codecov.yml`:

- **Target**: Auto (maintains current coverage level)
- **Threshold**: 1% (allows minor coverage decreases)
- **Range**: 70-100% (coverage quality scale)
- **Ignored paths**: Tests, gateway, UI code

## CMake Presets

### Coverage Preset

The `coverage` preset is configured for coverage collection:

```bash
# Configure
cmake --preset coverage

# Build all targets
cmake --build --preset coverage-all -j$(nproc)

# Run tests
ctest --preset coverage -j$(nproc)
```

### Coverage Build Options

- **Compiler**: GCC (better coverage support than Clang)
- **Build Type**: Debug (unoptimized for accurate coverage)
- **Flags**: `-fprofile-arcs -ftest-coverage -O0 -g`

## Viewing Coverage Reports

### On Codecov Dashboard

1. Go to https://codecov.io/gh/YOUR_USERNAME/VeloZ
2. View overall coverage percentage
3. Browse file-by-file coverage
4. See coverage trends over time
5. View pull request coverage diffs

### On GitHub

- Coverage badges appear in README.md
- PR comments show coverage changes
- Commit statuses indicate coverage pass/fail

### Locally

- Run `./scripts/coverage.sh`
- Open `coverage_html/index.html`
- Browse detailed line-by-line coverage

## Coverage Targets

Current coverage targets:

- **Project**: Maintain current coverage (auto target)
- **Patch**: New code should maintain coverage (auto target)
- **Threshold**: Allow up to 1% decrease

## Troubleshooting

### Coverage Not Uploading

1. Check that `CODECOV_TOKEN` is set in GitHub secrets
2. Verify the token is correct in Codecov dashboard
3. Check CI logs for upload errors

### Low Coverage Numbers

1. Ensure tests are actually running (`ctest` output)
2. Check that coverage flags are applied (`-fprofile-arcs -ftest-coverage`)
3. Verify `.gcda` files are generated in build directory

### Coverage Report Errors

1. Make sure `lcov` is installed
2. Check that paths in `codecov.yml` ignore patterns are correct
3. Verify build directory structure matches expected paths

## Best Practices

1. **Run coverage locally** before pushing to catch coverage decreases early
2. **Review coverage reports** for untested code paths
3. **Add tests** for new features to maintain coverage
4. **Check PR coverage** to ensure new code is tested
5. **Monitor trends** on Codecov dashboard to track coverage over time

## Additional Resources

- [Codecov Documentation](https://docs.codecov.com/)
- [lcov Documentation](http://ltp.sourceforge.net/coverage/lcov.php)
- [CMake Coverage Guide](https://cmake.org/cmake/help/latest/manual/cmake-ctest.1.html)
