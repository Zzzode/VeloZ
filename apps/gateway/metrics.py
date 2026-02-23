#!/usr/bin/env python3
"""
VeloZ Gateway Metrics Module

Provides Prometheus metrics collection and exposition for the Python gateway.
Uses prometheus_client library for metrics instrumentation.
"""

import time
import threading
from typing import Optional, Dict, Any
from dataclasses import dataclass, field
from collections import defaultdict

# Try to import prometheus_client, fall back to stub implementation
try:
    from prometheus_client import (
        Counter,
        Gauge,
        Histogram,
        Summary,
        generate_latest,
        CONTENT_TYPE_LATEST,
        CollectorRegistry,
        REGISTRY,
    )
    PROMETHEUS_AVAILABLE = True
except ImportError:
    PROMETHEUS_AVAILABLE = False
    # Stub implementations for when prometheus_client is not installed
    class StubMetric:
        def __init__(self, *args, **kwargs):
            pass
        def inc(self, *args, **kwargs):
            pass
        def dec(self, *args, **kwargs):
            pass
        def set(self, *args, **kwargs):
            pass
        def observe(self, *args, **kwargs):
            pass
        def labels(self, *args, **kwargs):
            return self
    Counter = Gauge = Histogram = Summary = StubMetric
    CONTENT_TYPE_LATEST = "text/plain"
    REGISTRY = None
    def generate_latest(registry=None):
        return b"# prometheus_client not installed\n"


# =============================================================================
# Metric Definitions
# =============================================================================

# HTTP Request Metrics
HTTP_REQUESTS_TOTAL = Counter(
    "veloz_http_requests_total",
    "Total HTTP requests",
    ["method", "endpoint", "status"]
)

HTTP_REQUEST_DURATION = Histogram(
    "veloz_http_request_duration_seconds",
    "HTTP request duration in seconds",
    ["method", "endpoint"],
    buckets=(0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0)
)

HTTP_REQUEST_SIZE = Histogram(
    "veloz_http_request_size_bytes",
    "HTTP request size in bytes",
    ["method", "endpoint"],
    buckets=(100, 500, 1000, 5000, 10000, 50000, 100000, 500000, 1000000)
)

HTTP_RESPONSE_SIZE = Histogram(
    "veloz_http_response_size_bytes",
    "HTTP response size in bytes",
    ["method", "endpoint"],
    buckets=(100, 500, 1000, 5000, 10000, 50000, 100000, 500000, 1000000)
)

# Gateway State Metrics
GATEWAY_UPTIME = Gauge(
    "veloz_gateway_uptime_seconds",
    "Gateway uptime in seconds"
)

ENGINE_RUNNING = Gauge(
    "veloz_engine_running",
    "Whether the engine process is running (1=running, 0=stopped)"
)

ACTIVE_CONNECTIONS = Gauge(
    "veloz_active_connections",
    "Number of active HTTP connections"
)

SSE_CLIENTS = Gauge(
    "veloz_sse_clients",
    "Number of connected SSE clients"
)

WEBSOCKET_CONNECTED = Gauge(
    "veloz_websocket_connected",
    "WebSocket connection status (1=connected, 0=disconnected)"
)

# Order Metrics
ORDERS_TOTAL = Counter(
    "veloz_orders_total",
    "Total orders submitted",
    ["side", "type"]
)

FILLS_TOTAL = Counter(
    "veloz_fills_total",
    "Total order fills",
    ["side"]
)

CANCELS_TOTAL = Counter(
    "veloz_cancels_total",
    "Total order cancellations"
)

ACTIVE_ORDERS = Gauge(
    "veloz_active_orders",
    "Number of active orders"
)

ORDER_LATENCY = Histogram(
    "veloz_order_latency_seconds",
    "Order submission to acknowledgment latency",
    buckets=(0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1.0)
)

# Market Data Metrics
MARKET_UPDATES_TOTAL = Counter(
    "veloz_market_updates_total",
    "Total market data updates received",
    ["type"]
)

ORDERBOOK_UPDATES_TOTAL = Counter(
    "veloz_orderbook_updates_total",
    "Total orderbook updates"
)

TRADE_UPDATES_TOTAL = Counter(
    "veloz_trade_updates_total",
    "Total trade updates"
)

MARKET_DATA_LAG = Gauge(
    "veloz_market_data_lag_ms",
    "Market data lag in milliseconds"
)

MARKET_PROCESSING_LATENCY = Histogram(
    "veloz_market_processing_latency_seconds",
    "Market data processing latency",
    buckets=(0.0001, 0.0005, 0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1)
)

# Risk Metrics
RISK_REJECTIONS_TOTAL = Counter(
    "veloz_risk_rejections_total",
    "Total orders rejected by risk checks",
    ["reason"]
)

RISK_UTILIZATION = Gauge(
    "veloz_risk_utilization_percent",
    "Risk limit utilization percentage"
)

POSITION_VALUE = Gauge(
    "veloz_position_value",
    "Position value in USD",
    ["symbol"]
)

# Exchange Connectivity Metrics
EXCHANGE_API_LATENCY = Histogram(
    "veloz_exchange_api_latency_seconds",
    "Exchange API call latency",
    ["exchange", "endpoint"],
    buckets=(0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0)
)

EXCHANGE_API_ERRORS = Counter(
    "veloz_exchange_api_errors_total",
    "Exchange API errors",
    ["exchange", "error_type"]
)

WEBSOCKET_RECONNECTS = Counter(
    "veloz_websocket_reconnects_total",
    "WebSocket reconnection attempts",
    ["exchange"]
)

# Error Metrics
ERRORS_TOTAL = Counter(
    "veloz_errors_total",
    "Total errors",
    ["type", "component"]
)


# =============================================================================
# Metrics Manager
# =============================================================================

@dataclass
class MetricsManager:
    """Manages metrics collection and exposition."""

    start_time: float = field(default_factory=time.time)
    _lock: threading.Lock = field(default_factory=threading.Lock)
    _order_timestamps: Dict[str, float] = field(default_factory=dict)

    def __post_init__(self):
        """Initialize metrics on startup."""
        ENGINE_RUNNING.set(0)
        ACTIVE_CONNECTIONS.set(0)
        SSE_CLIENTS.set(0)
        WEBSOCKET_CONNECTED.set(0)
        ACTIVE_ORDERS.set(0)
        RISK_UTILIZATION.set(0)

    def update_uptime(self):
        """Update the uptime gauge."""
        GATEWAY_UPTIME.set(time.time() - self.start_time)

    def record_request_start(self) -> float:
        """Record the start of an HTTP request. Returns start time."""
        ACTIVE_CONNECTIONS.inc()
        return time.time()

    def record_request_end(
        self,
        start_time: float,
        method: str,
        endpoint: str,
        status: int,
        request_size: int = 0,
        response_size: int = 0
    ):
        """Record the end of an HTTP request."""
        duration = time.time() - start_time

        # Normalize endpoint for metrics (remove IDs, etc.)
        normalized_endpoint = self._normalize_endpoint(endpoint)

        HTTP_REQUESTS_TOTAL.labels(
            method=method,
            endpoint=normalized_endpoint,
            status=str(status)
        ).inc()

        HTTP_REQUEST_DURATION.labels(
            method=method,
            endpoint=normalized_endpoint
        ).observe(duration)

        if request_size > 0:
            HTTP_REQUEST_SIZE.labels(
                method=method,
                endpoint=normalized_endpoint
            ).observe(request_size)

        if response_size > 0:
            HTTP_RESPONSE_SIZE.labels(
                method=method,
                endpoint=normalized_endpoint
            ).observe(response_size)

        ACTIVE_CONNECTIONS.dec()

    def _normalize_endpoint(self, endpoint: str) -> str:
        """Normalize endpoint path for metrics labeling."""
        # Remove query parameters
        if "?" in endpoint:
            endpoint = endpoint.split("?")[0]

        # Normalize common patterns
        parts = endpoint.strip("/").split("/")
        normalized_parts = []

        for i, part in enumerate(parts):
            # Replace UUIDs and numeric IDs with placeholders
            if self._looks_like_id(part):
                normalized_parts.append("{id}")
            else:
                normalized_parts.append(part)

        return "/" + "/".join(normalized_parts) if normalized_parts else "/"

    def _looks_like_id(self, s: str) -> bool:
        """Check if a string looks like an ID (UUID or numeric)."""
        if not s:
            return False
        # Check for UUID pattern
        if len(s) == 36 and s.count("-") == 4:
            return True
        # Check for numeric ID
        if s.isdigit() and len(s) > 4:
            return True
        return False

    def record_order_submitted(self, order_id: str, side: str, order_type: str):
        """Record an order submission."""
        with self._lock:
            self._order_timestamps[order_id] = time.time()
        ORDERS_TOTAL.labels(side=side, type=order_type).inc()
        ACTIVE_ORDERS.inc()

    def record_order_acknowledged(self, order_id: str):
        """Record order acknowledgment and calculate latency."""
        with self._lock:
            start_time = self._order_timestamps.pop(order_id, None)
        if start_time:
            latency = time.time() - start_time
            ORDER_LATENCY.observe(latency)

    def record_order_filled(self, order_id: str, side: str):
        """Record an order fill."""
        FILLS_TOTAL.labels(side=side).inc()
        ACTIVE_ORDERS.dec()
        # Clean up timestamp if still present
        with self._lock:
            self._order_timestamps.pop(order_id, None)

    def record_order_cancelled(self, order_id: str):
        """Record an order cancellation."""
        CANCELS_TOTAL.inc()
        ACTIVE_ORDERS.dec()
        # Clean up timestamp if still present
        with self._lock:
            self._order_timestamps.pop(order_id, None)

    def record_market_update(self, update_type: str):
        """Record a market data update."""
        MARKET_UPDATES_TOTAL.labels(type=update_type).inc()
        if update_type == "orderbook":
            ORDERBOOK_UPDATES_TOTAL.inc()
        elif update_type == "trade":
            TRADE_UPDATES_TOTAL.inc()

    def record_market_data_lag(self, lag_ms: float):
        """Record market data lag."""
        MARKET_DATA_LAG.set(lag_ms)

    def record_market_processing_latency(self, latency_seconds: float):
        """Record market data processing latency."""
        MARKET_PROCESSING_LATENCY.observe(latency_seconds)

    def record_risk_rejection(self, reason: str):
        """Record a risk rejection."""
        RISK_REJECTIONS_TOTAL.labels(reason=reason).inc()

    def set_risk_utilization(self, percent: float):
        """Set risk utilization percentage."""
        RISK_UTILIZATION.set(percent)

    def set_position_value(self, symbol: str, value: float):
        """Set position value for a symbol."""
        POSITION_VALUE.labels(symbol=symbol).set(value)

    def record_exchange_api_call(
        self,
        exchange: str,
        endpoint: str,
        latency_seconds: float,
        error: Optional[str] = None
    ):
        """Record an exchange API call."""
        EXCHANGE_API_LATENCY.labels(
            exchange=exchange,
            endpoint=endpoint
        ).observe(latency_seconds)

        if error:
            EXCHANGE_API_ERRORS.labels(
                exchange=exchange,
                error_type=error
            ).inc()

    def record_websocket_reconnect(self, exchange: str):
        """Record a WebSocket reconnection."""
        WEBSOCKET_RECONNECTS.labels(exchange=exchange).inc()

    def set_websocket_connected(self, connected: bool):
        """Set WebSocket connection status."""
        WEBSOCKET_CONNECTED.set(1 if connected else 0)

    def set_engine_running(self, running: bool):
        """Set engine running status."""
        ENGINE_RUNNING.set(1 if running else 0)

    def sse_client_connected(self):
        """Record SSE client connection."""
        SSE_CLIENTS.inc()

    def sse_client_disconnected(self):
        """Record SSE client disconnection."""
        SSE_CLIENTS.dec()

    def record_error(self, error_type: str, component: str):
        """Record an error."""
        ERRORS_TOTAL.labels(type=error_type, component=component).inc()

    def get_metrics(self) -> bytes:
        """Generate Prometheus metrics output."""
        self.update_uptime()
        return generate_latest(REGISTRY)

    def get_content_type(self) -> str:
        """Get the content type for metrics output."""
        return CONTENT_TYPE_LATEST


# Global metrics manager instance
_metrics_manager: Optional[MetricsManager] = None
_metrics_lock = threading.Lock()


def get_metrics_manager() -> MetricsManager:
    """Get or create the global metrics manager."""
    global _metrics_manager
    with _metrics_lock:
        if _metrics_manager is None:
            _metrics_manager = MetricsManager()
        return _metrics_manager


def init_metrics():
    """Initialize the metrics system."""
    return get_metrics_manager()


# =============================================================================
# Request Timing Context Manager
# =============================================================================

class RequestTimer:
    """Context manager for timing HTTP requests."""

    def __init__(self, method: str, endpoint: str):
        self.method = method
        self.endpoint = endpoint
        self.start_time: float = 0
        self.request_size: int = 0
        self.response_size: int = 0
        self.status: int = 200
        self._manager = get_metrics_manager()

    def __enter__(self):
        self.start_time = self._manager.record_request_start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        if exc_type is not None:
            self.status = 500
        self._manager.record_request_end(
            self.start_time,
            self.method,
            self.endpoint,
            self.status,
            self.request_size,
            self.response_size
        )
        return False


# =============================================================================
# Utility Functions
# =============================================================================

def metrics_available() -> bool:
    """Check if prometheus_client is available."""
    return PROMETHEUS_AVAILABLE
