# CI/CD Pipeline

This document describes the continuous integration and deployment pipeline for VeloZ.

## Overview

VeloZ uses GitHub Actions for CI/CD with the following stages:
1. **Build**: Compile and test
2. **Test**: Run unit and integration tests
3. **Security**: Scan for vulnerabilities
4. **Deploy**: Deploy to staging/production

## Pipeline Architecture

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│    Build    │────▶│    Test     │────▶│  Security   │────▶│   Deploy    │
│             │     │             │     │             │     │             │
│ - Configure │     │ - Unit      │     │ - SAST      │     │ - Staging   │
│ - Compile   │     │ - Integration│    │ - Deps scan │     │ - Production│
│ - Package   │     │ - Coverage  │     │ - Secrets   │     │ - Rollback  │
└─────────────┘     └─────────────┘     └─────────────┘     └─────────────┘
```

## GitHub Actions Workflow

### Main CI Workflow

```yaml
# .github/workflows/ci.yml
name: CI

on:
  push:
    branches: [main, master]
  pull_request:
    branches: [main, master]

env:
  BUILD_TYPE: Release

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        compiler: [gcc-13, clang-16]
        build_type: [Debug, Release]

    steps:
      - uses: actions/checkout@v4

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y \
            cmake ninja-build \
            ${{ matrix.compiler }} \
            libssl-dev

      - name: Configure CMake
        run: |
          cmake --preset dev \
            -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
            -DCMAKE_C_COMPILER=${{ matrix.compiler == 'gcc-13' && 'gcc-13' || 'clang-16' }} \
            -DCMAKE_CXX_COMPILER=${{ matrix.compiler == 'gcc-13' && 'g++-13' || 'clang++-16' }}

      - name: Build
        run: cmake --build --preset dev-all -j$(nproc)

      - name: Upload build artifacts
        uses: actions/upload-artifact@v4
        with:
          name: veloz-${{ matrix.compiler }}-${{ matrix.build_type }}
          path: build/dev/apps/engine/veloz_engine

  test:
    needs: build
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Download build artifacts
        uses: actions/download-artifact@v4
        with:
          name: veloz-gcc-13-Debug

      - name: Run unit tests
        run: ctest --preset dev -j$(nproc) --output-on-failure

      - name: Upload test results
        uses: actions/upload-artifact@v4
        if: always()
        with:
          name: test-results
          path: build/dev/Testing/

  coverage:
    needs: build
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Build with coverage
        run: |
          cmake --preset dev -DCMAKE_CXX_FLAGS="--coverage"
          cmake --build --preset dev-all -j$(nproc)

      - name: Run tests with coverage
        run: ctest --preset dev -j$(nproc)

      - name: Generate coverage report
        run: |
          gcovr --xml coverage.xml --root .

      - name: Upload coverage to Codecov
        uses: codecov/codecov-action@v4
        with:
          files: coverage.xml
          fail_ci_if_error: true

  security:
    needs: build
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Run CodeQL analysis
        uses: github/codeql-action/analyze@v3
        with:
          languages: cpp

      - name: Dependency scan
        uses: snyk/actions/cpp@master
        env:
          SNYK_TOKEN: ${{ secrets.SNYK_TOKEN }}

  docker:
    needs: [test, security]
    runs-on: ubuntu-latest
    if: github.ref == 'refs/heads/main'

    steps:
      - uses: actions/checkout@v4

      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3

      - name: Login to Docker Hub
        uses: docker/login-action@v3
        with:
          username: ${{ secrets.DOCKER_USERNAME }}
          password: ${{ secrets.DOCKER_PASSWORD }}

      - name: Build and push
        uses: docker/build-push-action@v5
        with:
          context: .
          push: true
          tags: |
            veloz/veloz:latest
            veloz/veloz:${{ github.sha }}
          cache-from: type=gha
          cache-to: type=gha,mode=max
```

### Deployment Workflow

```yaml
# .github/workflows/deploy.yml
name: Deploy

on:
  workflow_dispatch:
    inputs:
      environment:
        description: 'Deployment environment'
        required: true
        default: 'staging'
        type: choice
        options:
          - staging
          - production

jobs:
  deploy-staging:
    if: github.event.inputs.environment == 'staging'
    runs-on: ubuntu-latest
    environment: staging

    steps:
      - uses: actions/checkout@v4

      - name: Configure AWS credentials
        uses: aws-actions/configure-aws-credentials@v4
        with:
          aws-access-key-id: ${{ secrets.AWS_ACCESS_KEY_ID }}
          aws-secret-access-key: ${{ secrets.AWS_SECRET_ACCESS_KEY }}
          aws-region: us-east-1

      - name: Deploy to ECS
        run: |
          aws ecs update-service \
            --cluster veloz-staging \
            --service veloz-gateway \
            --force-new-deployment

      - name: Wait for deployment
        run: |
          aws ecs wait services-stable \
            --cluster veloz-staging \
            --services veloz-gateway

      - name: Run smoke tests
        run: |
          curl -f https://staging.veloz.example.com/health

  deploy-production:
    if: github.event.inputs.environment == 'production'
    runs-on: ubuntu-latest
    environment: production

    steps:
      - uses: actions/checkout@v4

      - name: Configure AWS credentials
        uses: aws-actions/configure-aws-credentials@v4
        with:
          aws-access-key-id: ${{ secrets.AWS_ACCESS_KEY_ID }}
          aws-secret-access-key: ${{ secrets.AWS_SECRET_ACCESS_KEY }}
          aws-region: us-east-1

      - name: Blue-green deployment
        run: |
          # Update task definition
          aws ecs register-task-definition \
            --cli-input-json file://ecs-task-definition.json

          # Update service with new task definition
          aws ecs update-service \
            --cluster veloz-production \
            --service veloz-gateway \
            --task-definition veloz-gateway:latest

      - name: Wait for deployment
        run: |
          aws ecs wait services-stable \
            --cluster veloz-production \
            --services veloz-gateway

      - name: Run smoke tests
        run: |
          curl -f https://veloz.example.com/health

      - name: Notify deployment
        uses: slackapi/slack-github-action@v1
        with:
          payload: |
            {
              "text": "VeloZ deployed to production",
              "blocks": [
                {
                  "type": "section",
                  "text": {
                    "type": "mrkdwn",
                    "text": "VeloZ *${{ github.sha }}* deployed to production"
                  }
                }
              ]
            }
        env:
          SLACK_WEBHOOK_URL: ${{ secrets.SLACK_WEBHOOK_URL }}
```

### Release Workflow

```yaml
# .github/workflows/release.yml
name: Release

on:
  push:
    tags:
      - 'v*'

jobs:
  release:
    runs-on: ubuntu-latest

    steps:
      - uses: actions/checkout@v4

      - name: Build release
        run: |
          cmake --preset release
          cmake --build --preset release-all -j$(nproc)

      - name: Create release archive
        run: |
          tar -czf veloz-${{ github.ref_name }}-linux-x64.tar.gz \
            -C build/release/apps/engine veloz_engine \
            -C ../../../ README.md LICENSE

      - name: Create GitHub release
        uses: softprops/action-gh-release@v1
        with:
          files: veloz-${{ github.ref_name }}-linux-x64.tar.gz
          generate_release_notes: true
```

## Branch Strategy

### Branch Types

| Branch | Purpose | Protection |
|--------|---------|------------|
| `main` | Production-ready code | Required reviews, CI pass |
| `develop` | Integration branch | CI pass |
| `feature/*` | Feature development | None |
| `hotfix/*` | Production fixes | Required reviews |
| `release/*` | Release preparation | Required reviews |

### Branch Protection Rules

```yaml
# Branch protection for main
branches:
  main:
    protection:
      required_status_checks:
        strict: true
        contexts:
          - build
          - test
          - security
      required_pull_request_reviews:
        required_approving_review_count: 1
        dismiss_stale_reviews: true
      enforce_admins: true
```

## Environment Configuration

### Staging Environment

```yaml
# environments/staging.yml
name: staging
url: https://staging.veloz.example.com
variables:
  VELOZ_EXECUTION_MODE: sim_engine
  VELOZ_MARKET_SOURCE: binance_rest
  VELOZ_LOG_LEVEL: debug
secrets:
  BINANCE_API_KEY: ${{ secrets.STAGING_BINANCE_API_KEY }}
  BINANCE_API_SECRET: ${{ secrets.STAGING_BINANCE_API_SECRET }}
```

### Production Environment

```yaml
# environments/production.yml
name: production
url: https://veloz.example.com
variables:
  VELOZ_EXECUTION_MODE: binance_testnet_spot
  VELOZ_MARKET_SOURCE: binance_rest
  VELOZ_LOG_LEVEL: info
secrets:
  BINANCE_API_KEY: ${{ secrets.PROD_BINANCE_API_KEY }}
  BINANCE_API_SECRET: ${{ secrets.PROD_BINANCE_API_SECRET }}
protection_rules:
  - required_reviewers: 2
  - wait_timer: 30  # minutes
```

## Rollback Procedures

### Automatic Rollback

```yaml
# In deployment workflow
- name: Health check
  id: health
  run: |
    for i in {1..5}; do
      if curl -f https://veloz.example.com/health; then
        echo "healthy=true" >> $GITHUB_OUTPUT
        exit 0
      fi
      sleep 10
    done
    echo "healthy=false" >> $GITHUB_OUTPUT
    exit 1

- name: Rollback on failure
  if: steps.health.outputs.healthy == 'false'
  run: |
    aws ecs update-service \
      --cluster veloz-production \
      --service veloz-gateway \
      --task-definition veloz-gateway:previous
```

### Manual Rollback

```bash
# Rollback to previous version
aws ecs update-service \
  --cluster veloz-production \
  --service veloz-gateway \
  --task-definition veloz-gateway:42  # specific version

# Or using kubectl
kubectl rollout undo deployment/veloz-gateway
```

## Secrets Management

### Required Secrets

| Secret | Description | Scope |
|--------|-------------|-------|
| `DOCKER_USERNAME` | Docker Hub username | Repository |
| `DOCKER_PASSWORD` | Docker Hub password | Repository |
| `AWS_ACCESS_KEY_ID` | AWS credentials | Environment |
| `AWS_SECRET_ACCESS_KEY` | AWS credentials | Environment |
| `BINANCE_API_KEY` | Binance API key | Environment |
| `BINANCE_API_SECRET` | Binance API secret | Environment |
| `SLACK_WEBHOOK_URL` | Slack notifications | Repository |
| `SNYK_TOKEN` | Security scanning | Repository |

### Adding Secrets

```bash
# Using GitHub CLI
gh secret set DOCKER_USERNAME --body "username"
gh secret set DOCKER_PASSWORD --body "password"

# Environment-specific secrets
gh secret set BINANCE_API_KEY --env staging --body "key"
```

## Monitoring CI/CD

### Pipeline Metrics

- Build duration
- Test pass rate
- Deployment frequency
- Lead time for changes
- Mean time to recovery

### Notifications

```yaml
# Slack notification on failure
- name: Notify on failure
  if: failure()
  uses: slackapi/slack-github-action@v1
  with:
    payload: |
      {
        "text": "CI failed for ${{ github.repository }}",
        "attachments": [
          {
            "color": "danger",
            "fields": [
              {"title": "Branch", "value": "${{ github.ref }}", "short": true},
              {"title": "Commit", "value": "${{ github.sha }}", "short": true}
            ]
          }
        ]
      }
  env:
    SLACK_WEBHOOK_URL: ${{ secrets.SLACK_WEBHOOK_URL }}
```

## Related Documents

- [Production Architecture](production_architecture.md)
- [Monitoring](monitoring.md)
- [Troubleshooting](troubleshooting.md)
