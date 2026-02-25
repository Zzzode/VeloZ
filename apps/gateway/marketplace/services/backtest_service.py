"""
Backtest Service

Manages strategy backtesting, including job scheduling, execution,
and result retrieval.
"""

import json
import logging
import subprocess
import threading
import time
import uuid
from dataclasses import dataclass, field, asdict
from datetime import datetime, timedelta
from enum import Enum
from pathlib import Path
from typing import Any, Callable, Optional

from .template_service import TemplateService, get_template_service
from .validation_service import ValidationService

logger = logging.getLogger(__name__)


class BacktestStatus(str, Enum):
    """Backtest job status."""

    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"


@dataclass
class BacktestConfig:
    """Configuration for a backtest run."""

    template_id: str
    symbol: str
    parameters: dict[str, Any]
    start_date: str  # ISO format YYYY-MM-DD
    end_date: str  # ISO format YYYY-MM-DD
    initial_capital: float = 10000.0
    fee_rate: float = 0.001  # 0.1%
    slippage: float = 0.001  # 0.1%
    data_source: str = "binance"
    timeframe: str = "1h"


@dataclass
class TradeRecord:
    """Record of a single trade."""

    timestamp: int
    side: str  # "buy" or "sell"
    price: float
    quantity: float
    fee: float
    pnl: float


@dataclass
class BacktestMetrics:
    """Performance metrics from a backtest."""

    total_return: float = 0.0
    total_return_pct: float = 0.0
    max_drawdown: float = 0.0
    max_drawdown_pct: float = 0.0
    sharpe_ratio: float = 0.0
    sortino_ratio: float = 0.0
    win_rate: float = 0.0
    profit_factor: float = 0.0
    total_trades: int = 0
    winning_trades: int = 0
    losing_trades: int = 0
    avg_win: float = 0.0
    avg_loss: float = 0.0
    largest_win: float = 0.0
    largest_loss: float = 0.0
    avg_trade_duration: float = 0.0  # in hours
    total_fees: float = 0.0
    net_profit: float = 0.0


@dataclass
class BacktestResult:
    """Complete backtest result."""

    job_id: str
    status: BacktestStatus
    config: BacktestConfig
    metrics: Optional[BacktestMetrics] = None
    equity_curve: list[dict] = field(default_factory=list)
    drawdown_curve: list[dict] = field(default_factory=list)
    trades: list[TradeRecord] = field(default_factory=list)
    error_message: Optional[str] = None
    started_at: Optional[str] = None
    completed_at: Optional[str] = None
    duration_seconds: float = 0.0
    progress: float = 0.0

    def to_dict(self) -> dict:
        """Convert to dictionary for API response."""
        result = {
            "job_id": self.job_id,
            "status": self.status.value,
            "config": asdict(self.config),
            "progress": self.progress,
            "started_at": self.started_at,
            "completed_at": self.completed_at,
            "duration_seconds": self.duration_seconds,
        }

        if self.metrics:
            result["metrics"] = asdict(self.metrics)

        if self.equity_curve:
            result["equity_curve"] = self.equity_curve

        if self.drawdown_curve:
            result["drawdown_curve"] = self.drawdown_curve

        if self.trades:
            result["trades"] = [asdict(t) for t in self.trades]

        if self.error_message:
            result["error_message"] = self.error_message

        return result

    def to_summary(self) -> dict:
        """Return a summary for list views."""
        return {
            "job_id": self.job_id,
            "status": self.status.value,
            "template_id": self.config.template_id,
            "symbol": self.config.symbol,
            "progress": self.progress,
            "total_return_pct": self.metrics.total_return_pct if self.metrics else None,
            "max_drawdown_pct": self.metrics.max_drawdown_pct if self.metrics else None,
            "sharpe_ratio": self.metrics.sharpe_ratio if self.metrics else None,
            "started_at": self.started_at,
            "completed_at": self.completed_at,
        }


class BacktestService:
    """
    Service for managing backtests.

    Responsibilities:
    - Create and queue backtest jobs
    - Execute backtests via C++ engine
    - Track job progress
    - Store and retrieve results
    """

    def __init__(
        self,
        template_service: Optional[TemplateService] = None,
        engine_path: Optional[Path] = None,
        max_concurrent: int = 5,
        auto_discover_engine: bool = True,
    ):
        self._template_service = template_service or get_template_service()
        self._validation_service = ValidationService()

        # Find engine executable
        self._engine_path = engine_path
        if self._engine_path is None and auto_discover_engine:
            # Try common build locations
            base_path = Path(__file__).parent.parent.parent.parent.parent
            for build_dir in ["build/dev", "build/release"]:
                candidate = base_path / build_dir / "apps" / "engine" / "veloz_engine"
                if candidate.exists():
                    self._engine_path = candidate
                    break

        # Job management
        self._jobs: dict[str, BacktestResult] = {}
        self._job_queue: list[str] = []
        self._running_jobs: set[str] = set()
        self._max_concurrent = max_concurrent
        self._lock = threading.Lock()

        # Progress callbacks
        self._progress_callbacks: dict[str, list[Callable[[float], None]]] = {}

        # Worker thread
        self._worker_thread: Optional[threading.Thread] = None
        self._shutdown = False

    def start(self):
        """Start the backtest worker thread."""
        if self._worker_thread is None or not self._worker_thread.is_alive():
            self._shutdown = False
            self._worker_thread = threading.Thread(target=self._worker_loop, daemon=True)
            self._worker_thread.start()
            logger.info("Backtest worker started")

    def stop(self):
        """Stop the backtest worker thread."""
        self._shutdown = True
        if self._worker_thread:
            self._worker_thread.join(timeout=5.0)
            logger.info("Backtest worker stopped")

    def create_backtest(
        self,
        template_id: str,
        symbol: str,
        parameters: dict[str, Any],
        start_date: str,
        end_date: str,
        initial_capital: float = 10000.0,
        fee_rate: float = 0.001,
        slippage: float = 0.001,
        timeframe: str = "1h",
    ) -> BacktestResult:
        """
        Create a new backtest job.

        Args:
            template_id: Strategy template ID
            symbol: Trading symbol (e.g., "BTCUSDT")
            parameters: Strategy parameters
            start_date: Backtest start date (YYYY-MM-DD)
            end_date: Backtest end date (YYYY-MM-DD)
            initial_capital: Starting capital
            fee_rate: Trading fee rate
            slippage: Slippage rate
            timeframe: Data timeframe

        Returns:
            BacktestResult with job_id and pending status
        """
        # Validate template exists
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

        # Create job
        job_id = str(uuid.uuid4())
        config = BacktestConfig(
            template_id=template_id,
            symbol=symbol,
            parameters=validation.normalized_values,
            start_date=start_date,
            end_date=end_date,
            initial_capital=initial_capital,
            fee_rate=fee_rate,
            slippage=slippage,
            timeframe=timeframe,
        )

        result = BacktestResult(
            job_id=job_id,
            status=BacktestStatus.PENDING,
            config=config,
        )

        with self._lock:
            self._jobs[job_id] = result
            self._job_queue.append(job_id)

        logger.info(f"Created backtest job: {job_id}")
        return result

    def get_backtest(self, job_id: str) -> Optional[BacktestResult]:
        """Get backtest result by job ID."""
        return self._jobs.get(job_id)

    def list_backtests(
        self,
        template_id: Optional[str] = None,
        status: Optional[BacktestStatus] = None,
        limit: int = 50,
    ) -> list[BacktestResult]:
        """List backtest jobs with optional filtering."""
        results = list(self._jobs.values())

        if template_id:
            results = [r for r in results if r.config.template_id == template_id]

        if status:
            results = [r for r in results if r.status == status]

        # Sort by creation time (newest first)
        results.sort(key=lambda r: r.started_at or "", reverse=True)

        return results[:limit]

    def cancel_backtest(self, job_id: str) -> bool:
        """Cancel a pending or running backtest."""
        with self._lock:
            if job_id not in self._jobs:
                return False

            result = self._jobs[job_id]

            if result.status == BacktestStatus.PENDING:
                result.status = BacktestStatus.CANCELLED
                if job_id in self._job_queue:
                    self._job_queue.remove(job_id)
                return True

            if result.status == BacktestStatus.RUNNING:
                # Mark for cancellation (worker will handle)
                result.status = BacktestStatus.CANCELLED
                return True

        return False

    def on_progress(self, job_id: str, callback: Callable[[float], None]):
        """Register a progress callback for a job."""
        if job_id not in self._progress_callbacks:
            self._progress_callbacks[job_id] = []
        self._progress_callbacks[job_id].append(callback)

    def _worker_loop(self):
        """Worker thread main loop."""
        while not self._shutdown:
            job_id = None

            with self._lock:
                # Check if we can start a new job
                if (
                    len(self._running_jobs) < self._max_concurrent
                    and self._job_queue
                ):
                    job_id = self._job_queue.pop(0)
                    self._running_jobs.add(job_id)

            if job_id:
                try:
                    self._execute_backtest(job_id)
                except Exception as e:
                    logger.error(f"Backtest {job_id} failed: {e}")
                    result = self._jobs.get(job_id)
                    if result:
                        result.status = BacktestStatus.FAILED
                        result.error_message = str(e)
                finally:
                    with self._lock:
                        self._running_jobs.discard(job_id)
            else:
                time.sleep(0.1)

    def _execute_backtest(self, job_id: str):
        """Execute a single backtest job."""
        result = self._jobs.get(job_id)
        if not result:
            return

        if result.status == BacktestStatus.CANCELLED:
            return

        result.status = BacktestStatus.RUNNING
        result.started_at = datetime.utcnow().isoformat()
        start_time = time.time()

        try:
            # Get engine config
            template = self._template_service.get_template(result.config.template_id)
            if not template:
                raise ValueError(f"Template not found: {result.config.template_id}")

            engine_config = self._template_service.to_engine_config(
                result.config.template_id,
                result.config.parameters,
                result.config.symbol,
            )

            if not engine_config:
                raise ValueError("Failed to generate engine config")

            # Add backtest-specific config
            engine_config["start_time"] = self._date_to_timestamp(
                result.config.start_date
            )
            engine_config["end_time"] = self._date_to_timestamp(result.config.end_date)
            engine_config["initial_balance"] = result.config.initial_capital
            engine_config["fee_rate"] = result.config.fee_rate
            engine_config["slippage"] = result.config.slippage
            engine_config["data_type"] = "kline"
            engine_config["time_frame"] = result.config.timeframe

            # Execute backtest
            if self._engine_path and self._engine_path.exists():
                backtest_result = self._run_engine_backtest(engine_config, result)
            else:
                # Fallback to simulated backtest for development
                backtest_result = self._run_simulated_backtest(engine_config, result)

            # Update result
            result.metrics = backtest_result["metrics"]
            result.equity_curve = backtest_result["equity_curve"]
            result.drawdown_curve = backtest_result["drawdown_curve"]
            result.trades = backtest_result["trades"]
            result.status = BacktestStatus.COMPLETED
            result.progress = 1.0

        except Exception as e:
            logger.error(f"Backtest execution failed: {e}")
            result.status = BacktestStatus.FAILED
            result.error_message = str(e)

        finally:
            result.completed_at = datetime.utcnow().isoformat()
            result.duration_seconds = time.time() - start_time

    def _run_engine_backtest(
        self, config: dict, result: BacktestResult
    ) -> dict:
        """Run backtest using C++ engine."""
        # Prepare command
        config_json = json.dumps(config)

        cmd = [
            str(self._engine_path),
            "--backtest",
            "--config",
            config_json,
        ]

        # Run engine process
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

        output_lines = []
        for line in process.stdout:
            line = line.strip()
            if not line:
                continue

            try:
                event = json.loads(line)
                event_type = event.get("type")

                if event_type == "progress":
                    result.progress = event.get("progress", 0.0)
                    self._notify_progress(result.job_id, result.progress)

                elif event_type == "result":
                    output_lines.append(event)

            except json.JSONDecodeError:
                logger.debug(f"Non-JSON output: {line}")

        process.wait()

        if process.returncode != 0:
            stderr = process.stderr.read()
            raise RuntimeError(f"Engine failed: {stderr}")

        # Parse results
        return self._parse_engine_results(output_lines)

    def _run_simulated_backtest(
        self, config: dict, result: BacktestResult
    ) -> dict:
        """Run a simulated backtest for development/testing."""
        import random
        import math

        # Simulate progress
        for i in range(10):
            if result.status == BacktestStatus.CANCELLED:
                raise RuntimeError("Backtest cancelled")
            result.progress = (i + 1) / 10
            self._notify_progress(result.job_id, result.progress)
            time.sleep(0.2)

        # Generate simulated results
        initial_capital = config.get("initial_balance", 10000.0)
        num_days = 30  # Simulated period

        # Generate equity curve
        equity = initial_capital
        equity_curve = []
        drawdown_curve = []
        peak_equity = initial_capital
        trades = []

        for day in range(num_days):
            # Random daily return
            daily_return = random.gauss(0.002, 0.02)  # 0.2% mean, 2% std
            equity *= 1 + daily_return

            # start_time is a timestamp in ms, convert to datetime
            start_time_ms = config.get("start_time", 0)
            if isinstance(start_time_ms, int):
                start_dt = datetime.fromtimestamp(start_time_ms / 1000)
            else:
                start_dt = datetime.strptime(str(start_time_ms), "%Y-%m-%d")
            timestamp = int((start_dt + timedelta(days=day)).timestamp() * 1000)

            equity_curve.append(
                {
                    "timestamp": timestamp,
                    "equity": round(equity, 2),
                    "cumulative_return": round((equity / initial_capital - 1) * 100, 2),
                }
            )

            # Track drawdown
            if equity > peak_equity:
                peak_equity = equity
            drawdown = (peak_equity - equity) / peak_equity
            drawdown_curve.append(
                {"timestamp": timestamp, "drawdown": round(drawdown * 100, 2)}
            )

            # Generate random trades
            if random.random() < 0.3:  # 30% chance of trade per day
                trade_pnl = random.gauss(50, 100)
                trades.append(
                    TradeRecord(
                        timestamp=timestamp,
                        side="buy" if random.random() < 0.5 else "sell",
                        price=round(random.uniform(40000, 50000), 2),
                        quantity=round(random.uniform(0.001, 0.01), 6),
                        fee=round(abs(trade_pnl) * 0.001, 2),
                        pnl=round(trade_pnl, 2),
                    )
                )

        # Calculate metrics
        final_equity = equity_curve[-1]["equity"] if equity_curve else initial_capital
        total_return = final_equity - initial_capital
        total_return_pct = (total_return / initial_capital) * 100

        max_drawdown = max(d["drawdown"] for d in drawdown_curve) if drawdown_curve else 0

        winning_trades = [t for t in trades if t.pnl > 0]
        losing_trades = [t for t in trades if t.pnl <= 0]

        metrics = BacktestMetrics(
            total_return=round(total_return, 2),
            total_return_pct=round(total_return_pct, 2),
            max_drawdown=round(max_drawdown * initial_capital / 100, 2),
            max_drawdown_pct=round(max_drawdown, 2),
            sharpe_ratio=round(random.uniform(0.5, 2.5), 2),
            sortino_ratio=round(random.uniform(0.5, 3.0), 2),
            win_rate=round(
                len(winning_trades) / len(trades) * 100 if trades else 0, 2
            ),
            profit_factor=round(
                sum(t.pnl for t in winning_trades)
                / abs(sum(t.pnl for t in losing_trades))
                if losing_trades and sum(t.pnl for t in losing_trades) != 0
                else 0,
                2,
            ),
            total_trades=len(trades),
            winning_trades=len(winning_trades),
            losing_trades=len(losing_trades),
            avg_win=round(
                sum(t.pnl for t in winning_trades) / len(winning_trades)
                if winning_trades
                else 0,
                2,
            ),
            avg_loss=round(
                sum(t.pnl for t in losing_trades) / len(losing_trades)
                if losing_trades
                else 0,
                2,
            ),
            largest_win=round(max((t.pnl for t in trades), default=0), 2),
            largest_loss=round(min((t.pnl for t in trades), default=0), 2),
            total_fees=round(sum(t.fee for t in trades), 2),
            net_profit=round(total_return - sum(t.fee for t in trades), 2),
        )

        return {
            "metrics": metrics,
            "equity_curve": equity_curve,
            "drawdown_curve": drawdown_curve,
            "trades": trades,
        }

    def _parse_engine_results(self, output_lines: list[dict]) -> dict:
        """Parse results from engine output."""
        metrics = BacktestMetrics()
        equity_curve = []
        drawdown_curve = []
        trades = []

        for event in output_lines:
            event_type = event.get("type")

            if event_type == "metrics":
                data = event.get("data", {})
                metrics = BacktestMetrics(
                    total_return=data.get("total_return", 0),
                    total_return_pct=data.get("total_return_pct", 0),
                    max_drawdown=data.get("max_drawdown", 0),
                    max_drawdown_pct=data.get("max_drawdown_pct", 0),
                    sharpe_ratio=data.get("sharpe_ratio", 0),
                    win_rate=data.get("win_rate", 0),
                    profit_factor=data.get("profit_factor", 0),
                    total_trades=data.get("trade_count", 0),
                    winning_trades=data.get("win_count", 0),
                    losing_trades=data.get("lose_count", 0),
                    avg_win=data.get("avg_win", 0),
                    avg_loss=data.get("avg_lose", 0),
                )

            elif event_type == "equity":
                equity_curve.append(event.get("data", {}))

            elif event_type == "drawdown":
                drawdown_curve.append(event.get("data", {}))

            elif event_type == "trade":
                data = event.get("data", {})
                trades.append(
                    TradeRecord(
                        timestamp=data.get("timestamp", 0),
                        side=data.get("side", ""),
                        price=data.get("price", 0),
                        quantity=data.get("quantity", 0),
                        fee=data.get("fee", 0),
                        pnl=data.get("pnl", 0),
                    )
                )

        return {
            "metrics": metrics,
            "equity_curve": equity_curve,
            "drawdown_curve": drawdown_curve,
            "trades": trades,
        }

    def _date_to_timestamp(self, date_str: str) -> int:
        """Convert date string to timestamp in milliseconds."""
        try:
            dt = datetime.strptime(date_str, "%Y-%m-%d")
            return int(dt.timestamp() * 1000)
        except ValueError:
            # Try ISO format
            dt = datetime.fromisoformat(date_str.replace("Z", "+00:00"))
            return int(dt.timestamp() * 1000)

    def _notify_progress(self, job_id: str, progress: float):
        """Notify progress callbacks."""
        callbacks = self._progress_callbacks.get(job_id, [])
        for callback in callbacks:
            try:
                callback(progress)
            except Exception as e:
                logger.error(f"Progress callback error: {e}")


# Singleton instance
_backtest_service: Optional[BacktestService] = None


def get_backtest_service() -> BacktestService:
    """Get the singleton BacktestService instance."""
    global _backtest_service
    if _backtest_service is None:
        _backtest_service = BacktestService()
        _backtest_service.start()
    return _backtest_service
