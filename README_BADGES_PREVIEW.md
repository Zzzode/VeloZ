# README Badges Preview

This document shows the badges that have been added to the VeloZ README.

## Badge Layout

The badges are organized in three rows for better visual organization:

### Row 1: Build Status & Quality Metrics
- **CI**: Shows GitHub Actions workflow status
- **codecov**: Shows code coverage percentage
- **C++**: Language version (C++23)
- **Python**: Python version requirement (3.10+)
- **CMake**: Build system version (3.24+)
- **License**: Apache 2.0

### Row 2: Project Info & Platform
- **Version**: Current version (1.0)
- **Status**: Production Ready
- **Platform**: Linux | macOS
- **Compiler**: GCC 13+ | Clang 16+

### Row 3: Technologies & Infrastructure
- **Docker**: Docker support
- **Kubernetes**: Kubernetes ready
- **Prometheus**: Monitoring integration
- **WebSocket**: Real-time data support

## Badge Colors

Badges use semantic colors:
- 🔵 **Blue**: Technologies (C++, Python, CMake, Docker, Kubernetes)
- 🟢 **Green**: Success/Ready status (License, Version, Status)
- 🟠 **Orange**: Build tools (Compiler, Prometheus)
- ⚫ **Grey**: Platform info

## Customization

To customize badges:

1. **Change colors**: Modify the color parameter in the badge URL
   - Example: `blue`, `green`, `red`, `orange`, `yellow`, `brightgreen`, `success`

2. **Add logos**: Use `&logo=<name>&logoColor=white`
   - Find logo names at: https://simpleicons.org/

3. **Change style**: Modify `style=flat`
   - Options: `flat`, `flat-square`, `plastic`, `for-the-badge`, `social`

4. **Dynamic badges**: Use shields.io dynamic badges
   - Example: `https://img.shields.io/github/stars/Zzzode/VeloZ?style=social`

## Additional Badge Ideas

You can add more badges if needed:

```markdown
<!-- GitHub Stats -->
[![Stars](https://img.shields.io/github/stars/Zzzode/VeloZ?style=social)](https://github.com/Zzzode/VeloZ/stargazers)
[![Forks](https://img.shields.io/github/forks/Zzzode/VeloZ?style=social)](https://github.com/Zzzode/VeloZ/network/members)
[![Issues](https://img.shields.io/github/issues/Zzzode/VeloZ)](https://github.com/Zzzode/VeloZ/issues)

<!-- Code Quality -->
[![Code Quality](https://img.shields.io/codacy/grade/PROJECT_ID?logo=codacy)](https://www.codacy.com/)
[![CodeFactor](https://www.codefactor.io/repository/github/Zzzode/VeloZ/badge)](https://www.codefactor.io/repository/github/Zzzode/VeloZ)

<!-- Documentation -->
[![Documentation](https://img.shields.io/badge/docs-latest-blue.svg)](https://github.com/Zzzode/VeloZ/tree/master/docs)
[![API Docs](https://img.shields.io/badge/API-docs-blue.svg)](https://github.com/Zzzode/VeloZ/blob/master/docs/api/http_api.md)

<!-- Dependencies -->
[![Dependencies](https://img.shields.io/badge/dependencies-up%20to%20date-brightgreen.svg)](https://github.com/Zzzode/VeloZ)

<!-- Community -->
[![Discord](https://img.shields.io/discord/YOUR_SERVER_ID?logo=discord&logoColor=white)](https://discord.gg/YOUR_INVITE)
[![Telegram](https://img.shields.io/badge/Telegram-Join-blue?logo=telegram)](https://t.me/YOUR_GROUP)

<!-- Performance -->
[![Performance](https://img.shields.io/badge/Performance-80k%20events%2Fsec-success.svg)](https://github.com/Zzzode/VeloZ)
[![Latency](https://img.shields.io/badge/Latency-P99%20%3C%201ms-success.svg)](https://github.com/Zzzode/VeloZ)

<!-- Security -->
[![Security](https://img.shields.io/badge/Security-Enterprise-success.svg?logo=security)](https://github.com/Zzzode/VeloZ)
[![SAST](https://img.shields.io/badge/SAST-Passing-success.svg)](https://github.com/Zzzode/VeloZ)
```

## Badge Resources

- **Shields.io**: https://shields.io/ (badge generator)
- **Simple Icons**: https://simpleicons.org/ (logo library)
- **Badge Examples**: https://github.com/badges/shields
- **Custom Badges**: https://shields.io/badges/static-badge

## Current Badge URLs

All badges are using `shields.io` service:
- Format: `https://img.shields.io/badge/<LABEL>-<MESSAGE>-<COLOR>.svg?style=<STYLE>&logo=<LOGO>`
- Dynamic: `https://github.com/<USER>/<REPO>/actions/workflows/<WORKFLOW>.yml/badge.svg`
- Codecov: `https://codecov.io/gh/<USER>/<REPO>/branch/<BRANCH>/graph/badge.svg`
