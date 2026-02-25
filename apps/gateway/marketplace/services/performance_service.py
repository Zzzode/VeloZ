"""
Performance Service

Tracks and aggregates strategy performance metrics for historical
analysis and comparison.
"""

import json
import logging
import threading
from dataclasses import dataclass, field, asdict
from datetime import datetime, timedelta
from enum import Enum
from pathlib import Path
from typing import Any, Optional

logger = logging.getLogger(__name__)


class TimeRange(str, Enum):
    """Time range for performance queries."""

    DAY = "1d"
    WEEK = "1w"
    MONTH = "1m"
    QUARTER = "3m"
    YEAR = "1y"
    ALL = "all"


@dataclass
class DailyPerformance:
    """Daily performance snapshot."""

    date: str  # YYYY-MM-DD
    starting_equity: float
    ending_equity: float
    pnl: float
    pnl_pct: float
    trades: int
    winning_trades: int
    losing_trades: int
    max_drawdown: float
    volume: float


@dataclass
class PerformanceSummary:
    """Aggregated performance summary."""

    total_return: float = 0.0
    total_return_pct: float = 0.0
    annualized_return: float = 0.0
    max_drawdown: float = 0.0
    max_drawdown_pct: float = 0.0
    sharpe_ratio: float = 0.0
    sortino_ratio: float = 0.0
    calmar_ratio: float = 0.0
    win_rate: float = 0.0
    profit_factor: float = 0.0
    total_trades: int = 0
    avg_trade_pnl: float = 0.0
    best_day: float = 0.0
    worst_day: float = 0.0
    avg_daily_pnl: float = 0.0
    volatility: float = 0.0
    trading_days: int = 0


@dataclass
class StrategyPerformance:
    """Complete performance record for a strategy."""

    deployment_id: str
    template_id: str
    symbol: str
    start_date: str
    end_date: Optional[str]
    initial_capital: float
    current_equity: float
    summary: PerformanceSummary = field(default_factory=PerformanceSummary)
    daily_history: list[DailyPerformance] = field(default_factory=list)
    equity_curve: list[dict] = field(default_factory=list)

    def to_dict(self) -> dict:
        """Convert to dictionary for API response."""
        return {
            "deployment_id": self.deployment_id,
            "template_id": self.template_id,
            "symbol": self.symbol,
            "start_date": self.start_date,
            "end_date": self.end_date,
            "initial_capital": self.initial_capital,
            "current_equity": self.current_equity,
            "summary": asdict(self.summary),
            "daily_history": [asdict(d) for d in self.daily_history],
            "equity_curve": self.equity_curve,
        }


@dataclass
class StrategyComparison:
    """Comparison of multiple strategies."""

    strategies: list[dict]  # List of strategy summaries
    benchmark: Optional[dict] = None  # Buy-and-hold benchmark
    best_performer: Optional[str] = None
    worst_performer: Optional[str] = None
    avg_return: float = 0.0
    avg_sharpe: float = 0.0


class PerformanceService:
    """
    Service for tracking and analyzing strategy performance.

    Responsibilities:
    - Store daily performance snapshots
    - Calculate performance metrics
    - Generate performance reports
    - Compare strategy performance
    """

    def __init__(self, data_dir: Optional[Path] = None):
        self._data_dir = data_dir or Path(__file__).parent.parent / "data" / "performance"
        self._data_dir.mkdir(parents=True, exist_ok=True)

        # In-memory cache
        self._performance_cache: dict[str, StrategyPerformance] = {}
        self._lock = threading.Lock()

    def record_daily_performance(
        self,
        deployment_id: str,
        date: str,
        starting_equity: float,
        ending_equity: float,
        trades: int,
        winning_trades: int,
        losing_trades: int,
        max_drawdown: float,
        volume: float,
    ):
        """
        Record daily performance for a deployment.

        Args:
            deployment_id: Deployment ID
            date: Date (YYYY-MM-DD)
            starting_equity: Equity at start of day
            ending_equity: Equity at end of day
            trades: Number of trades
            winning_trades: Number of winning trades
            losing_trades: Number of losing trades
            max_drawdown: Maximum drawdown during day
            volume: Total trading volume
        """
        pnl = ending_equity - starting_equity
        pnl_pct = (pnl / starting_equity * 100) if starting_equity > 0 else 0

        daily = DailyPerformance(
            date=date,
            starting_equity=starting_equity,
            ending_equity=ending_equity,
            pnl=round(pnl, 2),
            pnl_pct=round(pnl_pct, 2),
            trades=trades,
            winning_trades=winning_trades,
            losing_trades=losing_trades,
            max_drawdown=round(max_drawdown, 2),
            volume=round(volume, 2),
        )

        with self._lock:
            if deployment_id in self._performance_cache:
                perf = self._performance_cache[deployment_id]
                perf.daily_history.append(daily)
                perf.current_equity = ending_equity
                perf.equity_curve.append({
                    "date": date,
                    "equity": ending_equity,
                })
                self._recalculate_summary(perf)

        # Persist to disk
        self._save_daily_record(deployment_id, daily)

    def initialize_performance(
        self,
        deployment_id: str,
        template_id: str,
        symbol: str,
        initial_capital: float,
    ):
        """
        Initialize performance tracking for a new deployment.

        Args:
            deployment_id: Deployment ID
            template_id: Strategy template ID
            symbol: Trading symbol
            initial_capital: Starting capital
        """
        perf = StrategyPerformance(
            deployment_id=deployment_id,
            template_id=template_id,
            symbol=symbol,
            start_date=datetime.utcnow().strftime("%Y-%m-%d"),
            end_date=None,
            initial_capital=initial_capital,
            current_equity=initial_capital,
        )

        with self._lock:
            self._performance_cache[deployment_id] = perf

        logger.info(f"Initialized performance tracking for {deployment_id}")

    def finalize_performance(self, deployment_id: str):
        """
        Finalize performance tracking when deployment stops.

        Args:
            deployment_id: Deployment ID
        """
        with self._lock:
            if deployment_id in self._performance_cache:
                perf = self._performance_cache[deployment_id]
                perf.end_date = datetime.utcnow().strftime("%Y-%m-%d")
                self._save_performance(perf)

        logger.info(f"Finalized performance tracking for {deployment_id}")

    def get_performance(
        self,
        deployment_id: str,
        time_range: TimeRange = TimeRange.ALL,
    ) -> Optional[StrategyPerformance]:
        """
        Get performance data for a deployment.

        Args:
            deployment_id: Deployment ID
            time_range: Time range to filter

        Returns:
            StrategyPerformance or None if not found
        """
        # Try cache first
        perf = self._performance_cache.get(deployment_id)

        # Try loading from disk
        if not perf:
            perf = self._load_performance(deployment_id)
            if perf:
                with self._lock:
                    self._performance_cache[deployment_id] = perf

        if not perf:
            return None

        # Filter by time range
        if time_range != TimeRange.ALL:
            perf = self._filter_by_time_range(perf, time_range)

        return perf

    def get_summary(
        self,
        deployment_id: str,
        time_range: TimeRange = TimeRange.ALL,
    ) -> Optional[PerformanceSummary]:
        """Get performance summary for a deployment."""
        perf = self.get_performance(deployment_id, time_range)
        return perf.summary if perf else None

    def compare_strategies(
        self,
        deployment_ids: list[str],
        time_range: TimeRange = TimeRange.MONTH,
        include_benchmark: bool = True,
    ) -> StrategyComparison:
        """
        Compare performance of multiple strategies.

        Args:
            deployment_ids: List of deployment IDs to compare
            time_range: Time range for comparison
            include_benchmark: Include buy-and-hold benchmark

        Returns:
            StrategyComparison with aggregated data
        """
        strategies = []

        for dep_id in deployment_ids:
            perf = self.get_performance(dep_id, time_range)
            if perf:
                strategies.append({
                    "deployment_id": dep_id,
                    "template_id": perf.template_id,
                    "symbol": perf.symbol,
                    "total_return_pct": perf.summary.total_return_pct,
                    "sharpe_ratio": perf.summary.sharpe_ratio,
                    "max_drawdown_pct": perf.summary.max_drawdown_pct,
                    "win_rate": perf.summary.win_rate,
                    "total_trades": perf.summary.total_trades,
                })

        if not strategies:
            return StrategyComparison(strategies=[])

        # Find best/worst performers
        sorted_by_return = sorted(
            strategies, key=lambda s: s["total_return_pct"], reverse=True
        )
        best_performer = sorted_by_return[0]["deployment_id"]
        worst_performer = sorted_by_return[-1]["deployment_id"]

        # Calculate averages
        avg_return = sum(s["total_return_pct"] for s in strategies) / len(strategies)
        avg_sharpe = sum(s["sharpe_ratio"] for s in strategies) / len(strategies)

        return StrategyComparison(
            strategies=strategies,
            benchmark=None,  # TODO: Implement benchmark calculation
            best_performer=best_performer,
            worst_performer=worst_performer,
            avg_return=round(avg_return, 2),
            avg_sharpe=round(avg_sharpe, 2),
        )

    def get_leaderboard(
        self,
        time_range: TimeRange = TimeRange.MONTH,
        metric: str = "total_return_pct",
        limit: int = 10,
    ) -> list[dict]:
        """
        Get strategy leaderboard ranked by metric.

        Args:
            time_range: Time range for ranking
            metric: Metric to rank by
            limit: Maximum number of results

        Returns:
            List of strategy summaries ranked by metric
        """
        all_performances = []

        # Collect all performances
        for dep_id in self._performance_cache:
            perf = self.get_performance(dep_id, time_range)
            if perf and perf.summary.trading_days > 0:
                all_performances.append({
                    "deployment_id": dep_id,
                    "template_id": perf.template_id,
                    "symbol": perf.symbol,
                    "total_return_pct": perf.summary.total_return_pct,
                    "sharpe_ratio": perf.summary.sharpe_ratio,
                    "max_drawdown_pct": perf.summary.max_drawdown_pct,
                    "win_rate": perf.summary.win_rate,
                    "total_trades": perf.summary.total_trades,
                    "trading_days": perf.summary.trading_days,
                })

        # Sort by metric
        reverse = metric not in ("max_drawdown_pct",)  # Lower drawdown is better
        sorted_perfs = sorted(
            all_performances,
            key=lambda p: p.get(metric, 0),
            reverse=reverse,
        )

        return sorted_perfs[:limit]

    def _recalculate_summary(self, perf: StrategyPerformance):
        """Recalculate performance summary from daily history."""
        if not perf.daily_history:
            return

        history = perf.daily_history

        # Basic metrics
        total_return = perf.current_equity - perf.initial_capital
        total_return_pct = (total_return / perf.initial_capital * 100) if perf.initial_capital > 0 else 0

        # Calculate annualized return
        trading_days = len(history)
        if trading_days > 0:
            daily_return = total_return_pct / trading_days
            annualized_return = daily_return * 252  # Trading days per year
        else:
            annualized_return = 0

        # Max drawdown
        max_drawdown = max(d.max_drawdown for d in history) if history else 0
        max_drawdown_pct = (max_drawdown / perf.initial_capital * 100) if perf.initial_capital > 0 else 0

        # Trade statistics
        total_trades = sum(d.trades for d in history)
        winning_trades = sum(d.winning_trades for d in history)
        losing_trades = sum(d.losing_trades for d in history)
        win_rate = (winning_trades / total_trades * 100) if total_trades > 0 else 0

        # Daily PnL statistics
        daily_pnls = [d.pnl for d in history]
        best_day = max(daily_pnls) if daily_pnls else 0
        worst_day = min(daily_pnls) if daily_pnls else 0
        avg_daily_pnl = sum(daily_pnls) / len(daily_pnls) if daily_pnls else 0

        # Volatility (standard deviation of daily returns)
        daily_returns = [d.pnl_pct for d in history]
        if len(daily_returns) > 1:
            mean_return = sum(daily_returns) / len(daily_returns)
            variance = sum((r - mean_return) ** 2 for r in daily_returns) / (len(daily_returns) - 1)
            volatility = variance ** 0.5
        else:
            volatility = 0

        # Sharpe ratio (assuming risk-free rate of 0)
        if volatility > 0:
            sharpe_ratio = (annualized_return / 100) / (volatility * (252 ** 0.5) / 100)
        else:
            sharpe_ratio = 0

        # Sortino ratio (using only negative returns for downside deviation)
        negative_returns = [r for r in daily_returns if r < 0]
        if negative_returns:
            downside_variance = sum(r ** 2 for r in negative_returns) / len(negative_returns)
            downside_deviation = downside_variance ** 0.5
            if downside_deviation > 0:
                sortino_ratio = (annualized_return / 100) / (downside_deviation * (252 ** 0.5) / 100)
            else:
                sortino_ratio = 0
        else:
            sortino_ratio = sharpe_ratio  # No negative returns

        # Calmar ratio
        if max_drawdown_pct > 0:
            calmar_ratio = annualized_return / max_drawdown_pct
        else:
            calmar_ratio = 0

        # Update summary
        perf.summary = PerformanceSummary(
            total_return=round(total_return, 2),
            total_return_pct=round(total_return_pct, 2),
            annualized_return=round(annualized_return, 2),
            max_drawdown=round(max_drawdown, 2),
            max_drawdown_pct=round(max_drawdown_pct, 2),
            sharpe_ratio=round(sharpe_ratio, 2),
            sortino_ratio=round(sortino_ratio, 2),
            calmar_ratio=round(calmar_ratio, 2),
            win_rate=round(win_rate, 2),
            profit_factor=0,  # TODO: Calculate from trade data
            total_trades=total_trades,
            avg_trade_pnl=round(total_return / total_trades, 2) if total_trades > 0 else 0,
            best_day=round(best_day, 2),
            worst_day=round(worst_day, 2),
            avg_daily_pnl=round(avg_daily_pnl, 2),
            volatility=round(volatility, 2),
            trading_days=trading_days,
        )

    def _filter_by_time_range(
        self, perf: StrategyPerformance, time_range: TimeRange
    ) -> StrategyPerformance:
        """Filter performance data by time range."""
        now = datetime.utcnow()

        if time_range == TimeRange.DAY:
            cutoff = now - timedelta(days=1)
        elif time_range == TimeRange.WEEK:
            cutoff = now - timedelta(weeks=1)
        elif time_range == TimeRange.MONTH:
            cutoff = now - timedelta(days=30)
        elif time_range == TimeRange.QUARTER:
            cutoff = now - timedelta(days=90)
        elif time_range == TimeRange.YEAR:
            cutoff = now - timedelta(days=365)
        else:
            return perf

        cutoff_str = cutoff.strftime("%Y-%m-%d")

        # Filter daily history
        filtered_history = [d for d in perf.daily_history if d.date >= cutoff_str]
        filtered_equity = [e for e in perf.equity_curve if e["date"] >= cutoff_str]

        # Create filtered copy
        filtered = StrategyPerformance(
            deployment_id=perf.deployment_id,
            template_id=perf.template_id,
            symbol=perf.symbol,
            start_date=cutoff_str,
            end_date=perf.end_date,
            initial_capital=filtered_history[0].starting_equity if filtered_history else perf.initial_capital,
            current_equity=perf.current_equity,
            daily_history=filtered_history,
            equity_curve=filtered_equity,
        )

        # Recalculate summary for filtered data
        self._recalculate_summary(filtered)

        return filtered

    def _save_daily_record(self, deployment_id: str, daily: DailyPerformance):
        """Save daily performance record to disk."""
        file_path = self._data_dir / f"{deployment_id}_daily.jsonl"
        with open(file_path, "a") as f:
            f.write(json.dumps(asdict(daily)) + "\n")

    def _save_performance(self, perf: StrategyPerformance):
        """Save complete performance data to disk."""
        file_path = self._data_dir / f"{perf.deployment_id}.json"
        with open(file_path, "w") as f:
            json.dump(perf.to_dict(), f, indent=2)

    def _load_performance(self, deployment_id: str) -> Optional[StrategyPerformance]:
        """Load performance data from disk."""
        file_path = self._data_dir / f"{deployment_id}.json"
        if not file_path.exists():
            return None

        try:
            with open(file_path, "r") as f:
                data = json.load(f)

            return StrategyPerformance(
                deployment_id=data["deployment_id"],
                template_id=data["template_id"],
                symbol=data["symbol"],
                start_date=data["start_date"],
                end_date=data.get("end_date"),
                initial_capital=data["initial_capital"],
                current_equity=data["current_equity"],
                summary=PerformanceSummary(**data.get("summary", {})),
                daily_history=[DailyPerformance(**d) for d in data.get("daily_history", [])],
                equity_curve=data.get("equity_curve", []),
            )
        except Exception as e:
            logger.error(f"Failed to load performance data: {e}")
            return None


# Singleton instance
_performance_service: Optional[PerformanceService] = None


def get_performance_service() -> PerformanceService:
    """Get the singleton PerformanceService instance."""
    global _performance_service
    if _performance_service is None:
        _performance_service = PerformanceService()
    return _performance_service
