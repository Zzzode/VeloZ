#!/usr/bin/env python3
"""Integration tests for multi-exchange scenarios.

Tests the gateway's handling of multiple exchange configurations:
- Exchange adapter configuration
- Order routing across exchanges
- Position aggregation
- Error handling for exchange failures
- Failover scenarios

Note: These tests use mock implementations to specify expected behavior.
The actual implementations should be added to gateway.py.
"""

import json
import os
import sys
import threading
import time
import unittest
from dataclasses import dataclass, field
from typing import Optional, List, Dict, Any
from unittest.mock import MagicMock, patch

# Add gateway directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))


# Mock implementations for testing
# These define the expected API for multi-exchange support

@dataclass
class ExchangeConfig:
    """Configuration for an exchange connection."""
    venue: str
    api_key: str = ""
    api_secret: str = ""
    passphrase: str = ""
    testnet: bool = False

    @classmethod
    def from_env(cls, venue: str) -> Optional["ExchangeConfig"]:
        """Create config from environment variables."""
        prefix = f"VELOZ_{venue.upper()}_"
        api_key = os.environ.get(f"{prefix}API_KEY", "")
        api_secret = os.environ.get(f"{prefix}API_SECRET", "")
        passphrase = os.environ.get(f"{prefix}PASSPHRASE", "")
        testnet = os.environ.get(f"{prefix}TESTNET", "").lower() == "true"

        if not api_key:
            return None

        return cls(
            venue=venue,
            api_key=api_key,
            api_secret=api_secret,
            passphrase=passphrase,
            testnet=testnet,
        )

    def is_valid(self) -> bool:
        """Check if configuration is valid."""
        return bool(self.venue and self.api_key and self.api_secret)


@dataclass
class ConnectionResult:
    """Result of a connection attempt."""
    success: bool
    message: str = ""


@dataclass
class RouteDecision:
    """Decision for order routing."""
    venue: str
    expected_price: float = 0.0
    reason: str = ""


@dataclass
class OrderResult:
    """Result of an order submission."""
    success: bool
    message: str = ""
    order_id: str = ""


class ExchangeManager:
    """Manages connections to multiple exchanges."""

    def __init__(self):
        self._exchanges: Dict[str, ExchangeConfig] = {}
        self._status: Dict[str, Dict[str, Any]] = {}

    def register_exchange(self, config: ExchangeConfig):
        """Register an exchange configuration."""
        self._exchanges[config.venue] = config
        self._status[config.venue] = {
            "venue": config.venue,
            "connected": False,
            "rate_limited": False,
            "retry_after": 0,
        }

    def has_exchange(self, venue: str) -> bool:
        """Check if exchange is registered."""
        return venue in self._exchanges

    def get_exchanges(self) -> List[str]:
        """Get list of registered exchanges."""
        return list(self._exchanges.keys())

    def get_status(self, venue: str) -> Optional[Dict[str, Any]]:
        """Get status of an exchange."""
        return self._status.get(venue)

    def connect(self, venue: str) -> ConnectionResult:
        """Connect to an exchange."""
        if venue not in self._exchanges:
            return ConnectionResult(False, f"Unknown exchange: {venue}")

        config = self._exchanges[venue]
        if config.api_key == "invalid_key":
            return ConnectionResult(False, "Connection error: invalid credentials")

        self._status[venue]["connected"] = True
        return ConnectionResult(True, "Connected")

    def set_rate_limited(self, venue: str, limited: bool, retry_after: int = 0):
        """Set rate limit status for an exchange."""
        if venue in self._status:
            self._status[venue]["rate_limited"] = limited
            self._status[venue]["retry_after"] = retry_after

    def get_health_status(self) -> List[Dict[str, Any]]:
        """Get health status of all exchanges."""
        return [
            {
                "venue": venue,
                "status": "healthy" if status["connected"] else "disconnected",
                "latency": 10.0,  # Mock latency
            }
            for venue, status in self._status.items()
        ]


class OrderRouter:
    """Routes orders to exchanges based on strategy."""

    def __init__(self):
        self._strategy = "direct"
        self._venues: List[str] = []
        self._bbo: Dict[str, Dict[str, Dict[str, float]]] = {}  # venue -> symbol -> {bid, ask}
        self._latencies: Dict[str, float] = {}
        self._liquidity: Dict[str, Dict[str, Dict[str, float]]] = {}
        self._available: Dict[str, bool] = {}
        self._primary: str = ""
        self._fallbacks: List[str] = []
        self._round_robin_idx = 0

    def set_strategy(self, strategy: str):
        """Set routing strategy."""
        self._strategy = strategy

    def add_venue(self, venue: str):
        """Add a venue for routing."""
        if venue not in self._venues:
            self._venues.append(venue)
            self._available[venue] = True

    def set_primary(self, venue: str):
        """Set primary venue for failover strategy."""
        self._primary = venue
        self.add_venue(venue)

    def add_fallback(self, venue: str):
        """Add fallback venue for failover strategy."""
        self._fallbacks.append(venue)
        self.add_venue(venue)

    def set_available(self, venue: str, available: bool):
        """Set availability of a venue."""
        self._available[venue] = available

    def update_bbo(self, venue: str, symbol: str, bid: float, ask: float):
        """Update best bid/offer for a venue."""
        if venue not in self._bbo:
            self._bbo[venue] = {}
        self._bbo[venue][symbol] = {"bid": bid, "ask": ask}
        self.add_venue(venue)

    def update_liquidity(self, venue: str, symbol: str, bid_size: float, ask_size: float):
        """Update liquidity for a venue."""
        if venue not in self._liquidity:
            self._liquidity[venue] = {}
        self._liquidity[venue][symbol] = {"bid_size": bid_size, "ask_size": ask_size}
        self.add_venue(venue)

    def record_latency(self, venue: str, latency_ms: float):
        """Record latency for a venue."""
        self._latencies[venue] = latency_ms
        self.add_venue(venue)

    def select_route(self, order: Dict[str, Any]) -> RouteDecision:
        """Select route for an order."""
        # Direct routing to specified venue
        if "venue" in order:
            return RouteDecision(venue=order["venue"])

        symbol = order.get("symbol", "")
        side = order.get("side", "BUY")

        if self._strategy == "best_price":
            return self._select_best_price(symbol, side)
        elif self._strategy == "lowest_latency":
            return self._select_lowest_latency()
        elif self._strategy == "round_robin":
            return self._select_round_robin()
        elif self._strategy == "failover":
            return self._select_failover()
        else:
            # Default to first available venue
            if self._venues:
                return RouteDecision(venue=self._venues[0])
            return RouteDecision(venue="binance")

    def _select_best_price(self, symbol: str, side: str) -> RouteDecision:
        """Select venue with best price."""
        best_venue = None
        best_price = None

        for venue, symbols in self._bbo.items():
            if symbol in symbols:
                prices = symbols[symbol]
                if side == "BUY":
                    # For buy, want lowest ask
                    price = prices["ask"]
                    if best_price is None or price < best_price:
                        best_price = price
                        best_venue = venue
                else:
                    # For sell, want highest bid
                    price = prices["bid"]
                    if best_price is None or price > best_price:
                        best_price = price
                        best_venue = venue

        if best_venue:
            return RouteDecision(venue=best_venue, expected_price=best_price)
        return RouteDecision(venue=self._venues[0] if self._venues else "binance")

    def _select_lowest_latency(self) -> RouteDecision:
        """Select venue with lowest latency."""
        if not self._latencies:
            return RouteDecision(venue=self._venues[0] if self._venues else "binance")

        best_venue = min(self._latencies, key=self._latencies.get)
        return RouteDecision(venue=best_venue)

    def _select_round_robin(self) -> RouteDecision:
        """Select venue using round-robin."""
        if not self._venues:
            return RouteDecision(venue="binance")

        venue = self._venues[self._round_robin_idx % len(self._venues)]
        self._round_robin_idx += 1
        return RouteDecision(venue=venue)

    def _select_failover(self) -> RouteDecision:
        """Select venue using failover strategy."""
        if self._primary and self._available.get(self._primary, False):
            return RouteDecision(venue=self._primary)

        for fallback in self._fallbacks:
            if self._available.get(fallback, False):
                return RouteDecision(venue=fallback)

        return RouteDecision(venue=self._primary or "binance")

    def submit_order(self, order: Dict[str, Any]) -> OrderResult:
        """Submit an order."""
        # Simulate order rejection for large orders
        if order.get("qty", 0) > 100000:
            return OrderResult(False, "Order rejected: exceeds maximum size")
        return OrderResult(True, "Order submitted", order_id="test-order-1")

    def detect_arbitrage(self, symbol: str) -> Optional[Dict[str, Any]]:
        """Detect arbitrage opportunities."""
        if len(self._bbo) < 2:
            return None

        best_ask_venue = None
        best_ask = float("inf")
        best_bid_venue = None
        best_bid = 0.0

        for venue, symbols in self._bbo.items():
            if symbol in symbols:
                prices = symbols[symbol]
                if prices["ask"] < best_ask:
                    best_ask = prices["ask"]
                    best_ask_venue = venue
                if prices["bid"] > best_bid:
                    best_bid = prices["bid"]
                    best_bid_venue = venue

        if best_bid > best_ask and best_ask_venue != best_bid_venue:
            return {
                "buy_venue": best_ask_venue,
                "sell_venue": best_bid_venue,
                "spread": best_bid - best_ask,
            }
        return None

    def split_order(self, order: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Split order across venues based on liquidity."""
        symbol = order.get("symbol", "")
        side = order.get("side", "BUY")
        total_qty = order.get("qty", 0)

        splits = []
        remaining = total_qty

        for venue, symbols in self._liquidity.items():
            if symbol in symbols and remaining > 0:
                liq = symbols[symbol]
                available = liq["ask_size"] if side == "BUY" else liq["bid_size"]
                fill_qty = min(remaining, available)
                if fill_qty > 0:
                    splits.append({
                        "venue": venue,
                        "symbol": symbol,
                        "side": side,
                        "qty": fill_qty,
                    })
                    remaining -= fill_qty

        return splits


class PositionTracker:
    """Tracks positions across exchanges."""

    def __init__(self):
        self._positions: Dict[str, Dict[str, Dict[str, float]]] = {}

    def update_position(self, venue: str, symbol: str, quantity: float, avg_price: float):
        """Update position for a venue/symbol."""
        if venue not in self._positions:
            self._positions[venue] = {}
        self._positions[venue][symbol] = {
            "quantity": quantity,
            "avg_price": avg_price,
        }

    def get_position(self, venue: str, symbol: str) -> Optional[Dict[str, float]]:
        """Get position for a venue/symbol."""
        if venue in self._positions and symbol in self._positions[venue]:
            return self._positions[venue][symbol]
        return None

    def get_aggregated_position(self, symbol: str) -> Dict[str, Any]:
        """Get aggregated position across all venues."""
        total_qty = 0.0
        venues = []

        for venue, symbols in self._positions.items():
            if symbol in symbols:
                pos = symbols[symbol]
                total_qty += pos["quantity"]
                venues.append(venue)

        return {
            "total_quantity": total_qty,
            "venues": venues,
        }

    def get_net_position(self, symbol: str) -> Dict[str, Any]:
        """Get net position (long - short) across all venues."""
        net_qty = 0.0

        for venue, symbols in self._positions.items():
            if symbol in symbols:
                net_qty += symbols[symbol]["quantity"]

        return {"net_quantity": net_qty}

    def calculate_pnl(self, venue: str, symbol: str, current_price: float) -> Dict[str, float]:
        """Calculate P&L for a position."""
        pos = self.get_position(venue, symbol)
        if not pos:
            return {"unrealized_pnl": 0.0, "pnl_percent": 0.0}

        qty = pos["quantity"]
        avg_price = pos["avg_price"]
        unrealized_pnl = (current_price - avg_price) * qty
        pnl_percent = ((current_price - avg_price) / avg_price) * 100 if avg_price else 0.0

        return {
            "unrealized_pnl": unrealized_pnl,
            "pnl_percent": pnl_percent,
        }

    def get_risk_metrics(self, venue: str, symbol: str) -> Dict[str, float]:
        """Get risk metrics for a position."""
        pos = self.get_position(venue, symbol)
        if not pos:
            return {"notional_value": 0.0, "exposure": 0.0}

        qty = pos["quantity"]
        avg_price = pos["avg_price"]
        notional = abs(qty * avg_price)

        return {
            "notional_value": notional,
            "exposure": notional,
        }


def normalize_symbol(symbol: str, venue: str) -> str:
    """Normalize symbol format for a venue."""
    # Standard format is BTCUSDT
    # Some exchanges use BTC-USDT
    if venue in ("okx", "coinbase"):
        # Insert hyphen before USDT/USD/USDC
        for quote in ("USDT", "USDC", "USD", "EUR", "GBP"):
            if symbol.endswith(quote):
                base = symbol[:-len(quote)]
                return f"{base}-{quote}"
    return symbol


class TestExchangeConfiguration(unittest.TestCase):
    """Tests for exchange configuration handling."""

    def test_parse_binance_config(self):
        """Test parsing Binance configuration from environment."""
        with patch.dict(os.environ, {
            "VELOZ_BINANCE_API_KEY": "test_key",
            "VELOZ_BINANCE_API_SECRET": "test_secret",
            "VELOZ_BINANCE_TESTNET": "true",
        }):
            config = ExchangeConfig.from_env("binance")
            self.assertEqual(config.venue, "binance")
            self.assertEqual(config.api_key, "test_key")
            self.assertEqual(config.api_secret, "test_secret")
            self.assertTrue(config.testnet)

    def test_parse_okx_config(self):
        """Test parsing OKX configuration from environment."""
        with patch.dict(os.environ, {
            "VELOZ_OKX_API_KEY": "okx_key",
            "VELOZ_OKX_API_SECRET": "okx_secret",
            "VELOZ_OKX_PASSPHRASE": "okx_pass",
        }):
            config = ExchangeConfig.from_env("okx")
            self.assertEqual(config.venue, "okx")
            self.assertEqual(config.api_key, "okx_key")
            self.assertEqual(config.passphrase, "okx_pass")

    def test_parse_bybit_config(self):
        """Test parsing Bybit configuration from environment."""
        with patch.dict(os.environ, {
            "VELOZ_BYBIT_API_KEY": "bybit_key",
            "VELOZ_BYBIT_API_SECRET": "bybit_secret",
        }):
            config = ExchangeConfig.from_env("bybit")
            self.assertEqual(config.venue, "bybit")
            self.assertEqual(config.api_key, "bybit_key")

    def test_missing_config_returns_none(self):
        """Test that missing configuration returns None."""
        with patch.dict(os.environ, {}, clear=True):
            config = ExchangeConfig.from_env("binance")
            self.assertIsNone(config)

    def test_config_validation(self):
        """Test configuration validation."""
        config = ExchangeConfig(
            venue="binance",
            api_key="key",
            api_secret="secret",
        )
        self.assertTrue(config.is_valid())

        # Missing secret
        config_invalid = ExchangeConfig(
            venue="binance",
            api_key="key",
            api_secret="",
        )
        self.assertFalse(config_invalid.is_valid())


class TestExchangeManager(unittest.TestCase):
    """Tests for exchange manager functionality."""

    def setUp(self):
        self.manager = ExchangeManager()

    def test_register_exchange(self):
        """Test registering an exchange."""
        config = ExchangeConfig(
            venue="binance",
            api_key="test_key",
            api_secret="test_secret",
        )
        self.manager.register_exchange(config)
        self.assertTrue(self.manager.has_exchange("binance"))

    def test_register_multiple_exchanges(self):
        """Test registering multiple exchanges."""
        configs = [
            ExchangeConfig(venue="binance", api_key="key1", api_secret="secret1"),
            ExchangeConfig(venue="okx", api_key="key2", api_secret="secret2"),
            ExchangeConfig(venue="bybit", api_key="key3", api_secret="secret3"),
        ]
        for config in configs:
            self.manager.register_exchange(config)

        self.assertEqual(len(self.manager.get_exchanges()), 3)
        self.assertTrue(self.manager.has_exchange("binance"))
        self.assertTrue(self.manager.has_exchange("okx"))
        self.assertTrue(self.manager.has_exchange("bybit"))

    def test_get_exchange_status(self):
        """Test getting exchange status."""
        config = ExchangeConfig(
            venue="binance",
            api_key="test_key",
            api_secret="test_secret",
        )
        self.manager.register_exchange(config)

        status = self.manager.get_status("binance")
        self.assertIsNotNone(status)
        self.assertEqual(status["venue"], "binance")
        self.assertIn("connected", status)

    def test_unknown_exchange_status(self):
        """Test getting status of unknown exchange."""
        status = self.manager.get_status("unknown")
        self.assertIsNone(status)


class TestOrderRouter(unittest.TestCase):
    """Tests for order routing across exchanges."""

    def setUp(self):
        self.router = OrderRouter()

    def test_route_to_specific_exchange(self):
        """Test routing order to specific exchange."""
        order = {
            "symbol": "BTCUSDT",
            "side": "BUY",
            "qty": 0.1,
            "price": 50000.0,
            "venue": "binance",
        }
        route = self.router.select_route(order)
        self.assertEqual(route.venue, "binance")

    def test_best_price_routing(self):
        """Test best price routing strategy."""
        self.router.set_strategy("best_price")

        # Set up order book data
        self.router.update_bbo("binance", "BTCUSDT", bid=50000.0, ask=50100.0)
        self.router.update_bbo("okx", "BTCUSDT", bid=50050.0, ask=50080.0)

        # Buy order should route to exchange with lowest ask
        order = {"symbol": "BTCUSDT", "side": "BUY", "qty": 0.1}
        route = self.router.select_route(order)
        self.assertEqual(route.venue, "okx")
        self.assertEqual(route.expected_price, 50080.0)

        # Sell order should route to exchange with highest bid
        order = {"symbol": "BTCUSDT", "side": "SELL", "qty": 0.1}
        route = self.router.select_route(order)
        self.assertEqual(route.venue, "okx")
        self.assertEqual(route.expected_price, 50050.0)

    def test_lowest_latency_routing(self):
        """Test lowest latency routing strategy."""
        self.router.set_strategy("lowest_latency")

        # Record latencies
        self.router.record_latency("binance", 10.0)  # 10ms
        self.router.record_latency("okx", 20.0)  # 20ms
        self.router.record_latency("bybit", 5.0)  # 5ms

        order = {"symbol": "BTCUSDT", "side": "BUY", "qty": 0.1}
        route = self.router.select_route(order)
        self.assertEqual(route.venue, "bybit")

    def test_round_robin_routing(self):
        """Test round-robin routing strategy."""
        self.router.set_strategy("round_robin")
        self.router.add_venue("binance")
        self.router.add_venue("okx")

        order = {"symbol": "BTCUSDT", "side": "BUY", "qty": 0.1}

        # Should alternate between venues
        route1 = self.router.select_route(order)
        route2 = self.router.select_route(order)
        self.assertNotEqual(route1.venue, route2.venue)

    def test_failover_routing(self):
        """Test failover when primary exchange is unavailable."""
        self.router.set_strategy("failover")
        self.router.set_primary("binance")
        self.router.add_fallback("okx")
        self.router.add_fallback("bybit")

        # Primary available
        self.router.set_available("binance", True)
        order = {"symbol": "BTCUSDT", "side": "BUY", "qty": 0.1}
        route = self.router.select_route(order)
        self.assertEqual(route.venue, "binance")

        # Primary unavailable, should failover
        self.router.set_available("binance", False)
        route = self.router.select_route(order)
        self.assertEqual(route.venue, "okx")


class TestPositionTracker(unittest.TestCase):
    """Tests for position tracking across exchanges."""

    def setUp(self):
        self.tracker = PositionTracker()

    def test_track_single_position(self):
        """Test tracking a single position."""
        self.tracker.update_position(
            venue="binance",
            symbol="BTCUSDT",
            quantity=1.0,
            avg_price=50000.0,
        )

        position = self.tracker.get_position("binance", "BTCUSDT")
        self.assertIsNotNone(position)
        self.assertEqual(position["quantity"], 1.0)
        self.assertEqual(position["avg_price"], 50000.0)

    def test_aggregate_positions(self):
        """Test aggregating positions across exchanges."""
        # Position on Binance
        self.tracker.update_position(
            venue="binance",
            symbol="BTCUSDT",
            quantity=1.0,
            avg_price=50000.0,
        )

        # Position on OKX
        self.tracker.update_position(
            venue="okx",
            symbol="BTCUSDT",
            quantity=0.5,
            avg_price=51000.0,
        )

        aggregated = self.tracker.get_aggregated_position("BTCUSDT")
        self.assertEqual(aggregated["total_quantity"], 1.5)
        self.assertEqual(len(aggregated["venues"]), 2)

    def test_position_pnl_calculation(self):
        """Test P&L calculation for positions."""
        self.tracker.update_position(
            venue="binance",
            symbol="BTCUSDT",
            quantity=1.0,
            avg_price=50000.0,
        )

        # Current price is 52000
        pnl = self.tracker.calculate_pnl("binance", "BTCUSDT", current_price=52000.0)
        self.assertEqual(pnl["unrealized_pnl"], 2000.0)
        self.assertEqual(pnl["pnl_percent"], 4.0)

    def test_position_risk_metrics(self):
        """Test risk metrics for positions."""
        self.tracker.update_position(
            venue="binance",
            symbol="BTCUSDT",
            quantity=1.0,
            avg_price=50000.0,
        )

        metrics = self.tracker.get_risk_metrics("binance", "BTCUSDT")
        self.assertIn("notional_value", metrics)
        self.assertIn("exposure", metrics)


class TestSymbolNormalization(unittest.TestCase):
    """Tests for symbol normalization across exchanges."""

    def test_binance_symbol_format(self):
        """Test Binance symbol format (no separator)."""
        symbol = normalize_symbol("BTCUSDT", "binance")
        self.assertEqual(symbol, "BTCUSDT")

    def test_okx_symbol_format(self):
        """Test OKX symbol format (hyphen separator)."""
        symbol = normalize_symbol("BTCUSDT", "okx")
        self.assertEqual(symbol, "BTC-USDT")

    def test_bybit_symbol_format(self):
        """Test Bybit symbol format."""
        symbol = normalize_symbol("BTCUSDT", "bybit")
        self.assertEqual(symbol, "BTCUSDT")

    def test_coinbase_symbol_format(self):
        """Test Coinbase symbol format (hyphen separator)."""
        symbol = normalize_symbol("BTCUSDT", "coinbase")
        self.assertEqual(symbol, "BTC-USDT")


class TestExchangeErrorHandling(unittest.TestCase):
    """Tests for exchange error handling."""

    def test_connection_error_handling(self):
        """Test handling of connection errors."""
        manager = ExchangeManager()
        config = ExchangeConfig(
            venue="binance",
            api_key="invalid_key",
            api_secret="invalid_secret",
        )
        manager.register_exchange(config)

        # Simulate connection error
        result = manager.connect("binance")
        # Should not raise, should return error status
        self.assertFalse(result.success)
        self.assertIn("error", result.message.lower())

    def test_rate_limit_handling(self):
        """Test handling of rate limit errors."""
        manager = ExchangeManager()
        config = ExchangeConfig(
            venue="binance",
            api_key="test_key",
            api_secret="test_secret",
        )
        manager.register_exchange(config)

        # Simulate rate limit
        manager.set_rate_limited("binance", True, retry_after=60)

        status = manager.get_status("binance")
        self.assertTrue(status["rate_limited"])
        self.assertEqual(status["retry_after"], 60)

    def test_order_rejection_handling(self):
        """Test handling of order rejections."""
        router = OrderRouter()

        # Simulate order rejection
        order = {
            "symbol": "BTCUSDT",
            "side": "BUY",
            "qty": 1000000.0,  # Exceeds limits
            "price": 50000.0,
            "venue": "binance",
        }

        result = router.submit_order(order)
        self.assertFalse(result.success)
        self.assertIn("rejected", result.message.lower())


class TestMultiExchangeScenarios(unittest.TestCase):
    """End-to-end tests for multi-exchange scenarios."""

    def test_arbitrage_opportunity_detection(self):
        """Test detection of arbitrage opportunities."""
        router = OrderRouter()

        # Set up price discrepancy
        router.update_bbo("binance", "BTCUSDT", bid=50000.0, ask=50100.0)
        router.update_bbo("okx", "BTCUSDT", bid=50200.0, ask=50300.0)

        # Detect arbitrage: buy on Binance, sell on OKX
        opportunity = router.detect_arbitrage("BTCUSDT")
        self.assertIsNotNone(opportunity)
        self.assertEqual(opportunity["buy_venue"], "binance")
        self.assertEqual(opportunity["sell_venue"], "okx")
        self.assertEqual(opportunity["spread"], 100.0)  # 50200 - 50100

    def test_split_order_execution(self):
        """Test splitting large order across exchanges."""
        router = OrderRouter()
        router.set_strategy("split")

        # Set up liquidity
        router.update_liquidity("binance", "BTCUSDT", bid_size=0.5, ask_size=0.5)
        router.update_liquidity("okx", "BTCUSDT", bid_size=0.3, ask_size=0.3)

        # Large order that needs splitting
        order = {
            "symbol": "BTCUSDT",
            "side": "BUY",
            "qty": 0.6,  # More than any single exchange
        }

        splits = router.split_order(order)
        self.assertEqual(len(splits), 2)
        total_qty = sum(s["qty"] for s in splits)
        self.assertEqual(total_qty, 0.6)

    def test_cross_exchange_position_netting(self):
        """Test netting positions across exchanges."""
        tracker = PositionTracker()

        # Long on Binance
        tracker.update_position("binance", "BTCUSDT", quantity=1.0, avg_price=50000.0)

        # Short on OKX (negative quantity)
        tracker.update_position("okx", "BTCUSDT", quantity=-0.5, avg_price=51000.0)

        net = tracker.get_net_position("BTCUSDT")
        self.assertEqual(net["net_quantity"], 0.5)  # 1.0 - 0.5

    def test_exchange_health_monitoring(self):
        """Test monitoring health of multiple exchanges."""
        manager = ExchangeManager()

        # Register exchanges
        for venue in ["binance", "okx", "bybit"]:
            config = ExchangeConfig(
                venue=venue,
                api_key=f"{venue}_key",
                api_secret=f"{venue}_secret",
            )
            manager.register_exchange(config)

        # Get health status
        health = manager.get_health_status()
        self.assertEqual(len(health), 3)
        for venue_health in health:
            self.assertIn("venue", venue_health)
            self.assertIn("status", venue_health)
            self.assertIn("latency", venue_health)


if __name__ == "__main__":
    unittest.main(verbosity=2)
