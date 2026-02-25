"""
Tests for the Backtest Service
"""

import pytest
import time
from unittest.mock import MagicMock, patch

from ..services.backtest_service import (
    BacktestService,
    BacktestConfig,
    BacktestStatus,
    BacktestResult,
    BacktestMetrics,
)
from ..services.template_service import TemplateService, StrategyTemplate


class TestBacktestService:
    """Tests for BacktestService."""

    @pytest.fixture
    def mock_template_service(self):
        """Create a mock template service."""
        service = MagicMock(spec=TemplateService)

        # Create a mock template
        template = StrategyTemplate(
            id="grid-trading",
            name="Grid Trading",
            version="1.0.0",
            category="market_making",
            strategy_type="grid",
            parameters=[
                {"name": "grid_count", "type": "integer", "default": 10},
                {"name": "upper_price", "type": "price"},
                {"name": "lower_price", "type": "price"},
            ],
            _raw={
                "parameters": [
                    {"name": "grid_count", "type": "integer", "default": 10},
                    {"name": "upper_price", "type": "price"},
                    {"name": "lower_price", "type": "price"},
                ]
            },
        )

        service.get_template.return_value = template
        service.to_engine_config.return_value = {
            "strategy_name": "Grid",
            "symbol": "BTCUSDT",
            "parameters": {"grid_count": 10},
        }

        return service

    @pytest.fixture
    def mock_validation_service(self):
        """Create a mock validation service."""
        from ..services.validation_service import ValidationService, ValidationResult

        service = MagicMock(spec=ValidationService)
        service.validate_parameters.return_value = ValidationResult(
            valid=True,
            normalized_values={
                "grid_count": 10,
                "upper_price": 50000,
                "lower_price": 40000,
            },
        )
        return service

    @pytest.fixture
    def service(self, mock_template_service):
        """Create a BacktestService with mocked dependencies."""
        service = BacktestService(
            template_service=mock_template_service,
            engine_path=None,  # Use simulated backtest
            max_concurrent=2,
            auto_discover_engine=False,  # Don't auto-discover engine for tests
        )
        return service

    def test_create_backtest(self, service):
        """Test creating a backtest job."""
        result = service.create_backtest(
            template_id="grid-trading",
            symbol="BTCUSDT",
            parameters={
                "grid_count": 10,
                "upper_price": 50000,
                "lower_price": 40000,
            },
            start_date="2024-01-01",
            end_date="2024-01-31",
        )

        assert result is not None
        assert result.job_id is not None
        assert result.status == BacktestStatus.PENDING
        assert result.config.template_id == "grid-trading"
        assert result.config.symbol == "BTCUSDT"

    def test_create_backtest_invalid_template(self, service, mock_template_service):
        """Test creating backtest with invalid template."""
        mock_template_service.get_template.return_value = None

        with pytest.raises(ValueError, match="Template not found"):
            service.create_backtest(
                template_id="nonexistent",
                symbol="BTCUSDT",
                parameters={},
                start_date="2024-01-01",
                end_date="2024-01-31",
            )

    def test_get_backtest(self, service):
        """Test getting a backtest by ID."""
        # Create a backtest first
        created = service.create_backtest(
            template_id="grid-trading",
            symbol="BTCUSDT",
            parameters={
                "grid_count": 10,
                "upper_price": 50000,
                "lower_price": 40000,
            },
            start_date="2024-01-01",
            end_date="2024-01-31",
        )

        # Get it back
        result = service.get_backtest(created.job_id)

        assert result is not None
        assert result.job_id == created.job_id

    def test_get_nonexistent_backtest(self, service):
        """Test getting a non-existent backtest."""
        result = service.get_backtest("nonexistent-id")
        assert result is None

    def test_list_backtests(self, service):
        """Test listing backtests."""
        # Create some backtests
        service.create_backtest(
            template_id="grid-trading",
            symbol="BTCUSDT",
            parameters={
                "grid_count": 10,
                "upper_price": 50000,
                "lower_price": 40000,
            },
            start_date="2024-01-01",
            end_date="2024-01-31",
        )

        service.create_backtest(
            template_id="grid-trading",
            symbol="ETHUSDT",
            parameters={
                "grid_count": 10,
                "upper_price": 3000,
                "lower_price": 2000,
            },
            start_date="2024-01-01",
            end_date="2024-01-31",
        )

        results = service.list_backtests()

        assert len(results) == 2

    def test_list_backtests_with_filter(self, service):
        """Test filtering backtests by status."""
        service.create_backtest(
            template_id="grid-trading",
            symbol="BTCUSDT",
            parameters={
                "grid_count": 10,
                "upper_price": 50000,
                "lower_price": 40000,
            },
            start_date="2024-01-01",
            end_date="2024-01-31",
        )

        # All should be pending
        results = service.list_backtests(status=BacktestStatus.PENDING)
        assert len(results) == 1

        results = service.list_backtests(status=BacktestStatus.COMPLETED)
        assert len(results) == 0

    def test_cancel_backtest(self, service):
        """Test cancelling a backtest."""
        created = service.create_backtest(
            template_id="grid-trading",
            symbol="BTCUSDT",
            parameters={
                "grid_count": 10,
                "upper_price": 50000,
                "lower_price": 40000,
            },
            start_date="2024-01-01",
            end_date="2024-01-31",
        )

        success = service.cancel_backtest(created.job_id)
        assert success

        result = service.get_backtest(created.job_id)
        assert result.status == BacktestStatus.CANCELLED

    def test_backtest_execution(self, service):
        """Test backtest execution (simulated)."""
        service.start()

        try:
            created = service.create_backtest(
                template_id="grid-trading",
                symbol="BTCUSDT",
                parameters={
                    "grid_count": 10,
                    "upper_price": 50000,
                    "lower_price": 40000,
                },
                start_date="2024-01-01",
                end_date="2024-01-31",
            )

            # Wait for completion (simulated backtest takes ~2 seconds)
            max_wait = 20
            waited = 0
            while waited < max_wait:
                result = service.get_backtest(created.job_id)
                if result.status in (
                    BacktestStatus.COMPLETED,
                    BacktestStatus.FAILED,
                ):
                    break
                time.sleep(0.5)
                waited += 0.5

            result = service.get_backtest(created.job_id)
            assert result.status == BacktestStatus.COMPLETED
            assert result.metrics is not None
            assert len(result.equity_curve) > 0

        finally:
            service.stop()


class TestBacktestConfig:
    """Tests for BacktestConfig dataclass."""

    def test_create_config(self):
        """Test creating a backtest config."""
        config = BacktestConfig(
            template_id="grid-trading",
            symbol="BTCUSDT",
            parameters={"grid_count": 10},
            start_date="2024-01-01",
            end_date="2024-01-31",
        )

        assert config.template_id == "grid-trading"
        assert config.initial_capital == 10000.0  # default
        assert config.fee_rate == 0.001  # default


class TestBacktestResult:
    """Tests for BacktestResult dataclass."""

    def test_to_dict(self):
        """Test converting result to dict."""
        result = BacktestResult(
            job_id="test-123",
            status=BacktestStatus.COMPLETED,
            config=BacktestConfig(
                template_id="grid-trading",
                symbol="BTCUSDT",
                parameters={},
                start_date="2024-01-01",
                end_date="2024-01-31",
            ),
            metrics=BacktestMetrics(
                total_return=1000,
                total_return_pct=10.0,
                sharpe_ratio=1.5,
            ),
        )

        data = result.to_dict()

        assert data["job_id"] == "test-123"
        assert data["status"] == "completed"
        assert data["metrics"]["total_return"] == 1000

    def test_to_summary(self):
        """Test getting result summary."""
        result = BacktestResult(
            job_id="test-123",
            status=BacktestStatus.COMPLETED,
            config=BacktestConfig(
                template_id="grid-trading",
                symbol="BTCUSDT",
                parameters={},
                start_date="2024-01-01",
                end_date="2024-01-31",
            ),
            metrics=BacktestMetrics(
                total_return_pct=10.0,
                max_drawdown_pct=5.0,
                sharpe_ratio=1.5,
            ),
        )

        summary = result.to_summary()

        assert summary["job_id"] == "test-123"
        assert summary["total_return_pct"] == 10.0
        assert "equity_curve" not in summary  # not in summary
