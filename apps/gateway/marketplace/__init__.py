"""
Strategy Marketplace Module

This module provides the backend implementation for the VeloZ Strategy Marketplace,
enabling users to browse, configure, backtest, and deploy pre-built trading strategies.

Components:
- templates/: YAML strategy template definitions
- schemas/: JSON Schema for template validation
- services/: Business logic services
"""

from .services.template_service import TemplateService
from .services.backtest_service import BacktestService
from .services.deployment_service import DeploymentService
from .services.performance_service import PerformanceService

__all__ = [
    "TemplateService",
    "BacktestService",
    "DeploymentService",
    "PerformanceService",
]
