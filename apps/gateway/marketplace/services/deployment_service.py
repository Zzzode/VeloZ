"""
Deployment Service

Manages strategy deployment lifecycle including starting, stopping,
pausing, and monitoring live strategies.
"""

import json
import logging
import threading
import time
import uuid
from dataclasses import dataclass, field, asdict
from datetime import datetime
from enum import Enum
from typing import Any, Callable, Optional

from .template_service import TemplateService, get_template_service
from .validation_service import ValidationService

logger = logging.getLogger(__name__)


class DeploymentStatus(str, Enum):
    """Deployment status."""

    PENDING = "pending"
    STARTING = "starting"
    RUNNING = "running"
    PAUSED = "paused"
    STOPPING = "stopping"
    STOPPED = "stopped"
    ERROR = "error"


class DeploymentMode(str, Enum):
    """Deployment mode."""

    PAPER = "paper"  # Paper trading (simulated)
    LIVE = "live"  # Live trading (real money)


@dataclass
class DeploymentConfig:
    """Configuration for a strategy deployment."""

    template_id: str
    symbol: str
    parameters: dict[str, Any]
    mode: DeploymentMode = DeploymentMode.PAPER
    capital_allocation: float = 1000.0
    max_position_size: float = 0.0  # 0 = no limit
    stop_loss_pct: float = 0.0  # 0 = disabled
    take_profit_pct: float = 0.0  # 0 = disabled
    exchange: str = "binance"


@dataclass
class DeploymentMetrics:
    """Real-time metrics for a deployment."""

    total_pnl: float = 0.0
    total_pnl_pct: float = 0.0
    today_pnl: float = 0.0
    today_pnl_pct: float = 0.0
    unrealized_pnl: float = 0.0
    realized_pnl: float = 0.0
    total_trades: int = 0
    winning_trades: int = 0
    losing_trades: int = 0
    win_rate: float = 0.0
    current_position: float = 0.0
    current_position_value: float = 0.0
    avg_entry_price: float = 0.0
    max_drawdown: float = 0.0
    max_drawdown_pct: float = 0.0


@dataclass
class OpenPosition:
    """Open position information."""

    symbol: str
    side: str  # "long" or "short"
    quantity: float
    entry_price: float
    current_price: float
    unrealized_pnl: float
    unrealized_pnl_pct: float
    opened_at: str


@dataclass
class RecentTrade:
    """Recent trade information."""

    timestamp: str
    side: str
    price: float
    quantity: float
    pnl: float
    fee: float


@dataclass
class Deployment:
    """Represents a deployed strategy."""

    deployment_id: str
    config: DeploymentConfig
    status: DeploymentStatus
    strategy_name: str
    created_at: str
    started_at: Optional[str] = None
    stopped_at: Optional[str] = None
    metrics: DeploymentMetrics = field(default_factory=DeploymentMetrics)
    positions: list[OpenPosition] = field(default_factory=list)
    recent_trades: list[RecentTrade] = field(default_factory=list)
    error_message: Optional[str] = None
    engine_strategy_id: Optional[str] = None

    def to_dict(self) -> dict:
        """Convert to dictionary for API response."""
        return {
            "deployment_id": self.deployment_id,
            "config": asdict(self.config),
            "status": self.status.value,
            "strategy_name": self.strategy_name,
            "created_at": self.created_at,
            "started_at": self.started_at,
            "stopped_at": self.stopped_at,
            "metrics": asdict(self.metrics),
            "positions": [asdict(p) for p in self.positions],
            "recent_trades": [asdict(t) for t in self.recent_trades],
            "error_message": self.error_message,
        }

    def to_summary(self) -> dict:
        """Return a summary for list views."""
        return {
            "deployment_id": self.deployment_id,
            "template_id": self.config.template_id,
            "symbol": self.config.symbol,
            "mode": self.config.mode.value,
            "status": self.status.value,
            "strategy_name": self.strategy_name,
            "total_pnl": self.metrics.total_pnl,
            "total_pnl_pct": self.metrics.total_pnl_pct,
            "today_pnl": self.metrics.today_pnl,
            "created_at": self.created_at,
        }


class DeploymentService:
    """
    Service for managing strategy deployments.

    Responsibilities:
    - Deploy strategies to the engine
    - Start/stop/pause deployments
    - Track deployment metrics
    - Handle deployment lifecycle
    """

    def __init__(
        self,
        template_service: Optional[TemplateService] = None,
        engine_interface: Optional[Any] = None,
        max_deployments_per_user: int = 5,
    ):
        self._template_service = template_service or get_template_service()
        self._validation_service = ValidationService()
        self._engine_interface = engine_interface
        self._max_deployments = max_deployments_per_user

        # Deployment storage
        self._deployments: dict[str, Deployment] = {}
        self._lock = threading.Lock()

        # Event callbacks
        self._status_callbacks: dict[str, list[Callable[[DeploymentStatus], None]]] = {}
        self._metrics_callbacks: dict[str, list[Callable[[DeploymentMetrics], None]]] = {}

    def create_deployment(
        self,
        template_id: str,
        symbol: str,
        parameters: dict[str, Any],
        mode: DeploymentMode = DeploymentMode.PAPER,
        capital_allocation: float = 1000.0,
        exchange: str = "binance",
        backtest_required: bool = True,
        backtest_job_id: Optional[str] = None,
    ) -> Deployment:
        """
        Create a new strategy deployment.

        Args:
            template_id: Strategy template ID
            symbol: Trading symbol
            parameters: Strategy parameters
            mode: Paper or live trading
            capital_allocation: Capital to allocate
            exchange: Target exchange
            backtest_required: Whether backtest is required before live deployment
            backtest_job_id: ID of completed backtest (required for live mode)

        Returns:
            Created Deployment object
        """
        # Validate template
        template = self._template_service.get_template(template_id)
        if not template:
            raise ValueError(f"Template not found: {template_id}")

        # Validate parameters
        validation = self._validation_service.validate_parameters(
            template._raw, parameters
        )
        if not validation.valid:
            errors = [f"{e.parameter}: {e.message}" for e in validation.errors]
            raise ValueError(f"Invalid parameters: {'; '.join(errors)}")

        # Check backtest requirement for live mode
        if mode == DeploymentMode.LIVE and backtest_required:
            if not backtest_job_id:
                raise ValueError(
                    "Backtest is required before live deployment. "
                    "Please run a backtest first."
                )
            # TODO: Verify backtest was successful

        # Check deployment limits
        active_deployments = self._count_active_deployments()
        if active_deployments >= self._max_deployments:
            raise ValueError(
                f"Maximum deployments reached ({self._max_deployments}). "
                "Please stop an existing deployment first."
            )

        # Create deployment
        deployment_id = str(uuid.uuid4())
        config = DeploymentConfig(
            template_id=template_id,
            symbol=symbol,
            parameters=validation.normalized_values,
            mode=mode,
            capital_allocation=capital_allocation,
            exchange=exchange,
        )

        deployment = Deployment(
            deployment_id=deployment_id,
            config=config,
            status=DeploymentStatus.PENDING,
            strategy_name=template.name,
            created_at=datetime.utcnow().isoformat(),
        )

        with self._lock:
            self._deployments[deployment_id] = deployment

        logger.info(
            f"Created deployment: {deployment_id} ({template.name} on {symbol})"
        )
        return deployment

    def start_deployment(self, deployment_id: str) -> bool:
        """
        Start a deployment.

        Args:
            deployment_id: Deployment ID

        Returns:
            True if started successfully
        """
        deployment = self._deployments.get(deployment_id)
        if not deployment:
            raise ValueError(f"Deployment not found: {deployment_id}")

        if deployment.status not in (
            DeploymentStatus.PENDING,
            DeploymentStatus.STOPPED,
            DeploymentStatus.PAUSED,
        ):
            raise ValueError(
                f"Cannot start deployment in {deployment.status.value} status"
            )

        deployment.status = DeploymentStatus.STARTING
        self._notify_status(deployment_id, deployment.status)

        try:
            # Get engine config
            engine_config = self._template_service.to_engine_config(
                deployment.config.template_id,
                deployment.config.parameters,
                deployment.config.symbol,
            )

            if not engine_config:
                raise ValueError("Failed to generate engine config")

            # Add deployment-specific config
            engine_config["capital_allocation"] = deployment.config.capital_allocation
            engine_config["mode"] = deployment.config.mode.value
            engine_config["exchange"] = deployment.config.exchange

            # Start strategy in engine
            if self._engine_interface:
                strategy_id = self._engine_interface.start_strategy(engine_config)
                deployment.engine_strategy_id = strategy_id
            else:
                # Simulated start for development
                deployment.engine_strategy_id = f"sim_{deployment_id[:8]}"

            deployment.status = DeploymentStatus.RUNNING
            deployment.started_at = datetime.utcnow().isoformat()
            self._notify_status(deployment_id, deployment.status)

            logger.info(f"Started deployment: {deployment_id}")
            return True

        except Exception as e:
            logger.error(f"Failed to start deployment {deployment_id}: {e}")
            deployment.status = DeploymentStatus.ERROR
            deployment.error_message = str(e)
            self._notify_status(deployment_id, deployment.status)
            return False

    def stop_deployment(self, deployment_id: str) -> bool:
        """
        Stop a deployment.

        Args:
            deployment_id: Deployment ID

        Returns:
            True if stopped successfully
        """
        deployment = self._deployments.get(deployment_id)
        if not deployment:
            raise ValueError(f"Deployment not found: {deployment_id}")

        if deployment.status not in (
            DeploymentStatus.RUNNING,
            DeploymentStatus.PAUSED,
        ):
            raise ValueError(
                f"Cannot stop deployment in {deployment.status.value} status"
            )

        deployment.status = DeploymentStatus.STOPPING
        self._notify_status(deployment_id, deployment.status)

        try:
            # Stop strategy in engine
            if self._engine_interface and deployment.engine_strategy_id:
                self._engine_interface.stop_strategy(deployment.engine_strategy_id)

            deployment.status = DeploymentStatus.STOPPED
            deployment.stopped_at = datetime.utcnow().isoformat()
            self._notify_status(deployment_id, deployment.status)

            logger.info(f"Stopped deployment: {deployment_id}")
            return True

        except Exception as e:
            logger.error(f"Failed to stop deployment {deployment_id}: {e}")
            deployment.status = DeploymentStatus.ERROR
            deployment.error_message = str(e)
            self._notify_status(deployment_id, deployment.status)
            return False

    def pause_deployment(self, deployment_id: str) -> bool:
        """
        Pause a deployment.

        Args:
            deployment_id: Deployment ID

        Returns:
            True if paused successfully
        """
        deployment = self._deployments.get(deployment_id)
        if not deployment:
            raise ValueError(f"Deployment not found: {deployment_id}")

        if deployment.status != DeploymentStatus.RUNNING:
            raise ValueError(
                f"Cannot pause deployment in {deployment.status.value} status"
            )

        try:
            # Pause strategy in engine
            if self._engine_interface and deployment.engine_strategy_id:
                self._engine_interface.pause_strategy(deployment.engine_strategy_id)

            deployment.status = DeploymentStatus.PAUSED
            self._notify_status(deployment_id, deployment.status)

            logger.info(f"Paused deployment: {deployment_id}")
            return True

        except Exception as e:
            logger.error(f"Failed to pause deployment {deployment_id}: {e}")
            deployment.error_message = str(e)
            return False

    def resume_deployment(self, deployment_id: str) -> bool:
        """
        Resume a paused deployment.

        Args:
            deployment_id: Deployment ID

        Returns:
            True if resumed successfully
        """
        deployment = self._deployments.get(deployment_id)
        if not deployment:
            raise ValueError(f"Deployment not found: {deployment_id}")

        if deployment.status != DeploymentStatus.PAUSED:
            raise ValueError(
                f"Cannot resume deployment in {deployment.status.value} status"
            )

        try:
            # Resume strategy in engine
            if self._engine_interface and deployment.engine_strategy_id:
                self._engine_interface.resume_strategy(deployment.engine_strategy_id)

            deployment.status = DeploymentStatus.RUNNING
            self._notify_status(deployment_id, deployment.status)

            logger.info(f"Resumed deployment: {deployment_id}")
            return True

        except Exception as e:
            logger.error(f"Failed to resume deployment {deployment_id}: {e}")
            deployment.error_message = str(e)
            return False

    def get_deployment(self, deployment_id: str) -> Optional[Deployment]:
        """Get deployment by ID."""
        return self._deployments.get(deployment_id)

    def list_deployments(
        self,
        status: Optional[DeploymentStatus] = None,
        mode: Optional[DeploymentMode] = None,
        template_id: Optional[str] = None,
    ) -> list[Deployment]:
        """List deployments with optional filtering."""
        results = list(self._deployments.values())

        if status:
            results = [d for d in results if d.status == status]

        if mode:
            results = [d for d in results if d.config.mode == mode]

        if template_id:
            results = [d for d in results if d.config.template_id == template_id]

        # Sort by creation time (newest first)
        results.sort(key=lambda d: d.created_at, reverse=True)

        return results

    def update_parameters(
        self, deployment_id: str, parameters: dict[str, Any]
    ) -> bool:
        """
        Update parameters of a running deployment (hot-reload).

        Args:
            deployment_id: Deployment ID
            parameters: New parameter values

        Returns:
            True if updated successfully
        """
        deployment = self._deployments.get(deployment_id)
        if not deployment:
            raise ValueError(f"Deployment not found: {deployment_id}")

        if deployment.status != DeploymentStatus.RUNNING:
            raise ValueError("Can only update parameters of running deployments")

        # Validate new parameters
        template = self._template_service.get_template(deployment.config.template_id)
        if not template:
            raise ValueError("Template not found")

        validation = self._validation_service.validate_parameters(
            template._raw, parameters
        )
        if not validation.valid:
            errors = [f"{e.parameter}: {e.message}" for e in validation.errors]
            raise ValueError(f"Invalid parameters: {'; '.join(errors)}")

        try:
            # Update in engine
            if self._engine_interface and deployment.engine_strategy_id:
                self._engine_interface.update_parameters(
                    deployment.engine_strategy_id, validation.normalized_values
                )

            # Update local config
            deployment.config.parameters = validation.normalized_values

            logger.info(f"Updated parameters for deployment: {deployment_id}")
            return True

        except Exception as e:
            logger.error(f"Failed to update parameters: {e}")
            return False

    def update_metrics(self, deployment_id: str, metrics: DeploymentMetrics):
        """Update metrics for a deployment (called by engine)."""
        deployment = self._deployments.get(deployment_id)
        if deployment:
            deployment.metrics = metrics
            self._notify_metrics(deployment_id, metrics)

    def update_positions(self, deployment_id: str, positions: list[OpenPosition]):
        """Update positions for a deployment (called by engine)."""
        deployment = self._deployments.get(deployment_id)
        if deployment:
            deployment.positions = positions

    def add_trade(self, deployment_id: str, trade: RecentTrade):
        """Add a trade to deployment history (called by engine)."""
        deployment = self._deployments.get(deployment_id)
        if deployment:
            deployment.recent_trades.insert(0, trade)
            # Keep only last 100 trades
            deployment.recent_trades = deployment.recent_trades[:100]

    def on_status_change(
        self, deployment_id: str, callback: Callable[[DeploymentStatus], None]
    ):
        """Register a status change callback."""
        if deployment_id not in self._status_callbacks:
            self._status_callbacks[deployment_id] = []
        self._status_callbacks[deployment_id].append(callback)

    def on_metrics_update(
        self, deployment_id: str, callback: Callable[[DeploymentMetrics], None]
    ):
        """Register a metrics update callback."""
        if deployment_id not in self._metrics_callbacks:
            self._metrics_callbacks[deployment_id] = []
        self._metrics_callbacks[deployment_id].append(callback)

    def _count_active_deployments(self) -> int:
        """Count active (non-stopped) deployments."""
        return sum(
            1
            for d in self._deployments.values()
            if d.status
            not in (DeploymentStatus.STOPPED, DeploymentStatus.ERROR)
        )

    def _notify_status(self, deployment_id: str, status: DeploymentStatus):
        """Notify status callbacks."""
        callbacks = self._status_callbacks.get(deployment_id, [])
        for callback in callbacks:
            try:
                callback(status)
            except Exception as e:
                logger.error(f"Status callback error: {e}")

    def _notify_metrics(self, deployment_id: str, metrics: DeploymentMetrics):
        """Notify metrics callbacks."""
        callbacks = self._metrics_callbacks.get(deployment_id, [])
        for callback in callbacks:
            try:
                callback(metrics)
            except Exception as e:
                logger.error(f"Metrics callback error: {e}")


# Singleton instance
_deployment_service: Optional[DeploymentService] = None


def get_deployment_service() -> DeploymentService:
    """Get the singleton DeploymentService instance."""
    global _deployment_service
    if _deployment_service is None:
        _deployment_service = DeploymentService()
    return _deployment_service
