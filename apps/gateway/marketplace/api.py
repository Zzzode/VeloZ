"""
Strategy Marketplace REST API

Provides HTTP endpoints for the strategy marketplace functionality.
Integrates with the gateway's HTTP server.
"""

import json
import logging
from dataclasses import asdict
from typing import Any, Callable, Optional
from urllib.parse import parse_qs

from .services.template_service import TemplateService, get_template_service
from .services.backtest_service import (
    BacktestService,
    BacktestStatus,
    get_backtest_service,
)
from .services.deployment_service import (
    DeploymentService,
    DeploymentStatus,
    DeploymentMode,
    get_deployment_service,
)
from .services.performance_service import (
    PerformanceService,
    TimeRange,
    get_performance_service,
)

logger = logging.getLogger(__name__)


class MarketplaceAPI:
    """
    REST API handler for the strategy marketplace.

    Endpoints:
    - GET  /api/strategies                    - List strategy templates
    - GET  /api/strategies/{id}               - Get strategy template details
    - GET  /api/strategies/{id}/schema        - Get parameter schema for template
    - POST /api/strategies/{id}/validate      - Validate parameters
    - GET  /api/strategies/categories         - List categories

    - POST /api/backtests                     - Create backtest job
    - GET  /api/backtests                     - List backtest jobs
    - GET  /api/backtests/{id}                - Get backtest result
    - DELETE /api/backtests/{id}              - Cancel backtest

    - POST /api/deployments                   - Create deployment
    - GET  /api/deployments                   - List deployments
    - GET  /api/deployments/{id}              - Get deployment details
    - POST /api/deployments/{id}/start        - Start deployment
    - POST /api/deployments/{id}/stop         - Stop deployment
    - POST /api/deployments/{id}/pause        - Pause deployment
    - POST /api/deployments/{id}/resume       - Resume deployment
    - PUT  /api/deployments/{id}/parameters   - Update parameters

    - GET  /api/performance/{deployment_id}   - Get performance data
    - GET  /api/performance/compare           - Compare strategies
    - GET  /api/performance/leaderboard       - Get leaderboard
    """

    def __init__(
        self,
        template_service: Optional[TemplateService] = None,
        backtest_service: Optional[BacktestService] = None,
        deployment_service: Optional[DeploymentService] = None,
        performance_service: Optional[PerformanceService] = None,
    ):
        self._template_service = template_service or get_template_service()
        self._backtest_service = backtest_service or get_backtest_service()
        self._deployment_service = deployment_service or get_deployment_service()
        self._performance_service = performance_service or get_performance_service()

        # Load templates on init
        self._template_service.load_templates()

    def handle_request(
        self, method: str, path: str, query: dict, body: Optional[dict]
    ) -> tuple[int, dict]:
        """
        Handle an API request.

        Args:
            method: HTTP method (GET, POST, PUT, DELETE)
            path: Request path (e.g., /api/strategies)
            query: Query parameters
            body: Request body (for POST/PUT)

        Returns:
            Tuple of (status_code, response_body)
        """
        try:
            # Route request
            if path == "/api/strategies":
                if method == "GET":
                    return self._list_strategies(query)

            elif path == "/api/strategies/categories":
                if method == "GET":
                    return self._list_categories()

            elif path.startswith("/api/strategies/") and "/validate" in path:
                strategy_id = path.split("/")[3]
                if method == "POST":
                    return self._validate_parameters(strategy_id, body)

            elif path.startswith("/api/strategies/") and "/schema" in path:
                strategy_id = path.split("/")[3]
                if method == "GET":
                    return self._get_parameter_schema(strategy_id)

            elif path.startswith("/api/strategies/"):
                strategy_id = path.split("/")[3]
                if method == "GET":
                    return self._get_strategy(strategy_id)

            elif path == "/api/backtests":
                if method == "GET":
                    return self._list_backtests(query)
                elif method == "POST":
                    return self._create_backtest(body)

            elif path.startswith("/api/backtests/"):
                backtest_id = path.split("/")[3]
                if method == "GET":
                    return self._get_backtest(backtest_id)
                elif method == "DELETE":
                    return self._cancel_backtest(backtest_id)

            elif path == "/api/deployments":
                if method == "GET":
                    return self._list_deployments(query)
                elif method == "POST":
                    return self._create_deployment(body)

            elif path.startswith("/api/deployments/") and "/start" in path:
                deployment_id = path.split("/")[3]
                if method == "POST":
                    return self._start_deployment(deployment_id)

            elif path.startswith("/api/deployments/") and "/stop" in path:
                deployment_id = path.split("/")[3]
                if method == "POST":
                    return self._stop_deployment(deployment_id)

            elif path.startswith("/api/deployments/") and "/pause" in path:
                deployment_id = path.split("/")[3]
                if method == "POST":
                    return self._pause_deployment(deployment_id)

            elif path.startswith("/api/deployments/") and "/resume" in path:
                deployment_id = path.split("/")[3]
                if method == "POST":
                    return self._resume_deployment(deployment_id)

            elif path.startswith("/api/deployments/") and "/parameters" in path:
                deployment_id = path.split("/")[3]
                if method == "PUT":
                    return self._update_deployment_parameters(deployment_id, body)

            elif path.startswith("/api/deployments/"):
                deployment_id = path.split("/")[3]
                if method == "GET":
                    return self._get_deployment(deployment_id)

            elif path == "/api/performance/compare":
                if method == "GET":
                    return self._compare_performance(query)

            elif path == "/api/performance/leaderboard":
                if method == "GET":
                    return self._get_leaderboard(query)

            elif path.startswith("/api/performance/"):
                deployment_id = path.split("/")[3]
                if method == "GET":
                    return self._get_performance(deployment_id, query)

            return 404, {"error": "Not found", "path": path}

        except ValueError as e:
            return 400, {"error": str(e)}
        except Exception as e:
            logger.error(f"API error: {e}")
            return 500, {"error": "Internal server error"}

    # Strategy Template Endpoints

    def _list_strategies(self, query: dict) -> tuple[int, dict]:
        """List strategy templates with filtering."""
        category = query.get("category", [None])[0]
        difficulty = query.get("difficulty", [None])[0]
        risk_level = query.get("risk_level", [None])[0]
        search = query.get("search", [None])[0]
        tags = query.get("tags", [])

        templates = self._template_service.list_templates(
            category=category,
            difficulty=difficulty,
            risk_level=risk_level,
            search=search,
            tags=tags if tags else None,
        )

        return 200, {
            "strategies": [t.to_summary() for t in templates],
            "total": len(templates),
        }

    def _get_strategy(self, strategy_id: str) -> tuple[int, dict]:
        """Get strategy template details."""
        template = self._template_service.get_template(strategy_id)
        if not template:
            return 404, {"error": f"Strategy not found: {strategy_id}"}

        return 200, {"strategy": template.to_dict()}

    def _list_categories(self) -> tuple[int, dict]:
        """List strategy categories."""
        categories = self._template_service.get_categories()
        return 200, {"categories": categories}

    def _validate_parameters(
        self, strategy_id: str, body: Optional[dict]
    ) -> tuple[int, dict]:
        """Validate strategy parameters."""
        if not body:
            return 400, {"error": "Request body required"}

        parameters = body.get("parameters", {})
        result = self._template_service.validate_configuration(strategy_id, parameters)

        return 200, {
            "valid": result.valid,
            "errors": [
                {"parameter": e.parameter, "message": e.message, "code": e.code}
                for e in result.errors
            ],
            "warnings": result.warnings,
            "normalized_values": result.normalized_values,
        }

    def _get_parameter_schema(self, strategy_id: str) -> tuple[int, dict]:
        """Get JSON Schema for strategy parameters."""
        schema = self._template_service.get_parameter_schema(strategy_id)
        if not schema:
            return 404, {"error": f"Strategy not found: {strategy_id}"}

        return 200, {"schema": schema}

    # Backtest Endpoints

    def _create_backtest(self, body: Optional[dict]) -> tuple[int, dict]:
        """Create a new backtest job."""
        if not body:
            return 400, {"error": "Request body required"}

        required = ["template_id", "symbol", "parameters", "start_date", "end_date"]
        for field in required:
            if field not in body:
                return 400, {"error": f"Missing required field: {field}"}

        result = self._backtest_service.create_backtest(
            template_id=body["template_id"],
            symbol=body["symbol"],
            parameters=body["parameters"],
            start_date=body["start_date"],
            end_date=body["end_date"],
            initial_capital=body.get("initial_capital", 10000.0),
            fee_rate=body.get("fee_rate", 0.001),
            slippage=body.get("slippage", 0.001),
            timeframe=body.get("timeframe", "1h"),
        )

        return 201, {"backtest": result.to_summary()}

    def _list_backtests(self, query: dict) -> tuple[int, dict]:
        """List backtest jobs."""
        template_id = query.get("template_id", [None])[0]
        status_str = query.get("status", [None])[0]
        limit = int(query.get("limit", [50])[0])

        status = BacktestStatus(status_str) if status_str else None

        results = self._backtest_service.list_backtests(
            template_id=template_id,
            status=status,
            limit=limit,
        )

        return 200, {
            "backtests": [r.to_summary() for r in results],
            "total": len(results),
        }

    def _get_backtest(self, backtest_id: str) -> tuple[int, dict]:
        """Get backtest result."""
        result = self._backtest_service.get_backtest(backtest_id)
        if not result:
            return 404, {"error": f"Backtest not found: {backtest_id}"}

        return 200, {"backtest": result.to_dict()}

    def _cancel_backtest(self, backtest_id: str) -> tuple[int, dict]:
        """Cancel a backtest job."""
        success = self._backtest_service.cancel_backtest(backtest_id)
        if not success:
            return 400, {"error": "Cannot cancel backtest"}

        return 200, {"message": "Backtest cancelled"}

    # Deployment Endpoints

    def _create_deployment(self, body: Optional[dict]) -> tuple[int, dict]:
        """Create a new deployment."""
        if not body:
            return 400, {"error": "Request body required"}

        required = ["template_id", "symbol", "parameters"]
        for field in required:
            if field not in body:
                return 400, {"error": f"Missing required field: {field}"}

        mode_str = body.get("mode", "paper")
        mode = DeploymentMode(mode_str)

        deployment = self._deployment_service.create_deployment(
            template_id=body["template_id"],
            symbol=body["symbol"],
            parameters=body["parameters"],
            mode=mode,
            capital_allocation=body.get("capital_allocation", 1000.0),
            exchange=body.get("exchange", "binance"),
            backtest_required=body.get("backtest_required", True),
            backtest_job_id=body.get("backtest_job_id"),
        )

        return 201, {"deployment": deployment.to_summary()}

    def _list_deployments(self, query: dict) -> tuple[int, dict]:
        """List deployments."""
        status_str = query.get("status", [None])[0]
        mode_str = query.get("mode", [None])[0]
        template_id = query.get("template_id", [None])[0]

        status = DeploymentStatus(status_str) if status_str else None
        mode = DeploymentMode(mode_str) if mode_str else None

        deployments = self._deployment_service.list_deployments(
            status=status,
            mode=mode,
            template_id=template_id,
        )

        return 200, {
            "deployments": [d.to_summary() for d in deployments],
            "total": len(deployments),
        }

    def _get_deployment(self, deployment_id: str) -> tuple[int, dict]:
        """Get deployment details."""
        deployment = self._deployment_service.get_deployment(deployment_id)
        if not deployment:
            return 404, {"error": f"Deployment not found: {deployment_id}"}

        return 200, {"deployment": deployment.to_dict()}

    def _start_deployment(self, deployment_id: str) -> tuple[int, dict]:
        """Start a deployment."""
        success = self._deployment_service.start_deployment(deployment_id)
        if not success:
            return 400, {"error": "Failed to start deployment"}

        return 200, {"message": "Deployment started"}

    def _stop_deployment(self, deployment_id: str) -> tuple[int, dict]:
        """Stop a deployment."""
        success = self._deployment_service.stop_deployment(deployment_id)
        if not success:
            return 400, {"error": "Failed to stop deployment"}

        return 200, {"message": "Deployment stopped"}

    def _pause_deployment(self, deployment_id: str) -> tuple[int, dict]:
        """Pause a deployment."""
        success = self._deployment_service.pause_deployment(deployment_id)
        if not success:
            return 400, {"error": "Failed to pause deployment"}

        return 200, {"message": "Deployment paused"}

    def _resume_deployment(self, deployment_id: str) -> tuple[int, dict]:
        """Resume a deployment."""
        success = self._deployment_service.resume_deployment(deployment_id)
        if not success:
            return 400, {"error": "Failed to resume deployment"}

        return 200, {"message": "Deployment resumed"}

    def _update_deployment_parameters(
        self, deployment_id: str, body: Optional[dict]
    ) -> tuple[int, dict]:
        """Update deployment parameters."""
        if not body:
            return 400, {"error": "Request body required"}

        parameters = body.get("parameters", {})
        success = self._deployment_service.update_parameters(deployment_id, parameters)
        if not success:
            return 400, {"error": "Failed to update parameters"}

        return 200, {"message": "Parameters updated"}

    # Performance Endpoints

    def _get_performance(
        self, deployment_id: str, query: dict
    ) -> tuple[int, dict]:
        """Get performance data for a deployment."""
        time_range_str = query.get("time_range", ["all"])[0]
        time_range = TimeRange(time_range_str)

        performance = self._performance_service.get_performance(
            deployment_id, time_range
        )
        if not performance:
            return 404, {"error": f"Performance data not found: {deployment_id}"}

        return 200, {"performance": performance.to_dict()}

    def _compare_performance(self, query: dict) -> tuple[int, dict]:
        """Compare performance of multiple strategies."""
        deployment_ids = query.get("deployment_ids", [])
        time_range_str = query.get("time_range", ["1m"])[0]
        time_range = TimeRange(time_range_str)

        if not deployment_ids:
            return 400, {"error": "deployment_ids required"}

        comparison = self._performance_service.compare_strategies(
            deployment_ids, time_range
        )

        return 200, {
            "comparison": {
                "strategies": comparison.strategies,
                "benchmark": comparison.benchmark,
                "best_performer": comparison.best_performer,
                "worst_performer": comparison.worst_performer,
                "avg_return": comparison.avg_return,
                "avg_sharpe": comparison.avg_sharpe,
            }
        }

    def _get_leaderboard(self, query: dict) -> tuple[int, dict]:
        """Get strategy leaderboard."""
        time_range_str = query.get("time_range", ["1m"])[0]
        metric = query.get("metric", ["total_return_pct"])[0]
        limit = int(query.get("limit", [10])[0])

        time_range = TimeRange(time_range_str)

        leaderboard = self._performance_service.get_leaderboard(
            time_range, metric, limit
        )

        return 200, {"leaderboard": leaderboard}


# Singleton instance
_marketplace_api: Optional[MarketplaceAPI] = None


def get_marketplace_api() -> MarketplaceAPI:
    """Get the singleton MarketplaceAPI instance."""
    global _marketplace_api
    if _marketplace_api is None:
        _marketplace_api = MarketplaceAPI()
    return _marketplace_api
