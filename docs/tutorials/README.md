# VeloZ Tutorials

Hands-on tutorials to help you learn VeloZ step by step.

## Available Tutorials

| Tutorial | Time | Level | Description |
|----------|------|-------|-------------|
| [Your First Trade](./first-trade.md) | 15 min | Beginner | Place your first order, monitor status, manage positions |
| [Grid Trading Strategy](./grid-trading.md) | 30 min | Intermediate | Configure and deploy grid trading strategies |
| [Market Making Strategy](./market-making.md) | 35 min | Intermediate | Set up market making with inventory management |
| [Multi-Exchange Arbitrage](./multi-exchange-arbitrage.md) | 25 min | Intermediate | Trade across multiple exchanges with smart routing |
| [Risk Management in Practice](./risk-management-practice.md) | 30 min | Intermediate | Configure position limits, circuit breakers, and VaR |
| [Performance Tuning](./performance-tuning-practice.md) | 35 min | Intermediate | Optimize latency and throughput |
| [Troubleshooting Common Issues](./troubleshooting-practice.md) | 25 min | Intermediate | Debug connection, order, and performance problems |
| [Production Deployment](./production-deployment.md) | 45 min | Intermediate | Deploy VeloZ to production with security hardening |
| [Custom Strategy Development](./custom-strategy-development.md) | 60 min | Advanced | Develop and deploy custom C++ trading strategies |

## Recommended Learning Paths

### For New Users
1. [Your First Trade](./first-trade.md) - Learn the basics
2. [Risk Management in Practice](./risk-management-practice.md) - Set up safety controls
3. [Troubleshooting Common Issues](./troubleshooting-practice.md) - Handle common problems
4. [Production Deployment](./production-deployment.md) - Deploy to production

### For Strategy Developers
1. [Your First Trade](./first-trade.md) - Understand order flow
2. [Grid Trading Strategy](./grid-trading.md) - Learn passive strategies
3. [Market Making Strategy](./market-making.md) - Understand market making
4. [Custom Strategy Development](./custom-strategy-development.md) - Build custom strategies
5. See [Risk Management Guide](../guides/user/risk-management.md) for production risk controls

### For Multi-Exchange Traders
1. [Your First Trade](./first-trade.md) - Single exchange basics
2. [Multi-Exchange Arbitrage](./multi-exchange-arbitrage.md) - Cross-exchange trading
3. [Risk Management in Practice](./risk-management-practice.md) - Multi-exchange risk controls
4. See [Trading Guide](../guides/user/trading-guide.md) for advanced features

### For Operations Teams
1. [Production Deployment](./production-deployment.md) - Deployment procedures
2. [Performance Tuning](./performance-tuning-practice.md) - Optimize system performance
3. [Troubleshooting Common Issues](./troubleshooting-practice.md) - Debug production issues
4. See [Operations Runbook](../guides/deployment/operations_runbook.md) for day-to-day operations

## Prerequisites

Before starting any tutorial:

1. VeloZ built successfully (see [Getting Started](../guides/user/getting-started.md))
2. Terminal access with `curl` installed
3. Basic understanding of cryptocurrency trading

## Tutorial Conventions

All tutorials follow these conventions:

- **Time estimates** are for first-time completion
- **Commands** use `curl` for API access
- **Expected outputs** show realistic responses
- **Troubleshooting** sections address common issues

See [Tutorial Style Guide](./STYLE_GUIDE.md) for detailed conventions.

## Additional Resources

### Reference Documentation
- [User Guide Index](../guides/user/README.md) - Complete reference documentation
- [Trading Guide](../guides/user/trading-guide.md) - Trading concepts and best practices
- [Risk Management Guide](../guides/user/risk-management.md) - Risk controls and VaR models
- [Monitoring Guide](../guides/user/monitoring.md) - Observability and alerting
- [API Reference](../api/README.md) - HTTP and SSE API documentation

### Detailed Guides
For in-depth information on specific topics covered in tutorials:
- **Grid Trading**: See [Trading Guide - Grid Trading Section](../guides/user/trading-guide.md)
- **Market Making**: See [Trading Guide - Market Making Section](../guides/user/trading-guide.md)
- **Risk Management**: See [Risk Management Guide](../guides/user/risk-management.md)
- **Performance**: See [Latency Optimization Guide](../performance/latency_optimization.md)
- **Monitoring**: See [Monitoring Guide](../guides/user/monitoring.md)
- **Troubleshooting**: See [Troubleshooting Guide](../guides/user/troubleshooting.md)
- **Glossary**: See [Glossary](../guides/user/glossary.md) - Definitions of trading and technical terms
