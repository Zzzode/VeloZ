#!/usr/bin/env python3
"""
VeloZ Gateway Structured Logging Module

Provides JSON-formatted structured logging for log aggregation with Loki.
Integrates with OpenTelemetry for trace context propagation.
"""

import json
import logging
import sys
import time
import os
from typing import Optional, Dict, Any
from datetime import datetime, timezone

# Try to import OpenTelemetry for trace context
try:
    from opentelemetry import trace
    OTEL_AVAILABLE = True
except ImportError:
    OTEL_AVAILABLE = False


# =============================================================================
# JSON Formatter
# =============================================================================

class JSONFormatter(logging.Formatter):
    """
    JSON log formatter for structured logging.
    Outputs logs in a format optimized for Loki/Promtail ingestion.
    """

    def __init__(
        self,
        service_name: str = "veloz-gateway",
        include_trace: bool = True,
        include_extra: bool = True,
    ):
        super().__init__()
        self.service_name = service_name
        self.include_trace = include_trace
        self.include_extra = include_extra

    def format(self, record: logging.LogRecord) -> str:
        """Format log record as JSON."""
        log_entry = {
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "level": record.levelname,
            "logger": record.name,
            "message": record.getMessage(),
            "service": self.service_name,
            "component": getattr(record, "component", record.name.split(".")[-1]),
        }

        # Add source location
        log_entry["source"] = {
            "file": record.filename,
            "line": record.lineno,
            "function": record.funcName,
        }

        # Add trace context if available
        if self.include_trace and OTEL_AVAILABLE:
            span = trace.get_current_span()
            if span and span.is_recording():
                ctx = span.get_span_context()
                if ctx.is_valid:
                    log_entry["trace_id"] = format(ctx.trace_id, "032x")
                    log_entry["span_id"] = format(ctx.span_id, "016x")

        # Add exception info if present
        if record.exc_info:
            log_entry["exception"] = {
                "type": record.exc_info[0].__name__ if record.exc_info[0] else None,
                "message": str(record.exc_info[1]) if record.exc_info[1] else None,
                "traceback": self.formatException(record.exc_info),
            }

        # Add extra fields from record
        if self.include_extra:
            extra_fields = {}
            for key, value in record.__dict__.items():
                if key not in (
                    "name", "msg", "args", "created", "filename", "funcName",
                    "levelname", "levelno", "lineno", "module", "msecs",
                    "pathname", "process", "processName", "relativeCreated",
                    "stack_info", "exc_info", "exc_text", "thread", "threadName",
                    "message", "component",
                ):
                    try:
                        json.dumps(value)  # Check if serializable
                        extra_fields[key] = value
                    except (TypeError, ValueError):
                        extra_fields[key] = str(value)

            if extra_fields:
                log_entry["extra"] = extra_fields

        return json.dumps(log_entry, default=str)


# =============================================================================
# Structured Logger
# =============================================================================

class StructuredLogger:
    """
    Wrapper for creating structured loggers with consistent configuration.
    """

    def __init__(
        self,
        name: str,
        level: int = logging.INFO,
        service_name: str = "veloz-gateway",
        output: str = "stdout",  # stdout, stderr, or file path
    ):
        self.logger = logging.getLogger(name)
        self.logger.setLevel(level)
        self.logger.propagate = False

        # Remove existing handlers
        self.logger.handlers.clear()

        # Create JSON formatter
        formatter = JSONFormatter(service_name=service_name)

        # Create handler based on output
        if output == "stdout":
            handler = logging.StreamHandler(sys.stdout)
        elif output == "stderr":
            handler = logging.StreamHandler(sys.stderr)
        else:
            handler = logging.FileHandler(output)

        handler.setFormatter(formatter)
        self.logger.addHandler(handler)

    def debug(self, message: str, **kwargs):
        self._log(logging.DEBUG, message, **kwargs)

    def info(self, message: str, **kwargs):
        self._log(logging.INFO, message, **kwargs)

    def warning(self, message: str, **kwargs):
        self._log(logging.WARNING, message, **kwargs)

    def error(self, message: str, **kwargs):
        self._log(logging.ERROR, message, **kwargs)

    def critical(self, message: str, **kwargs):
        self._log(logging.CRITICAL, message, **kwargs)

    def exception(self, message: str, **kwargs):
        self._log(logging.ERROR, message, exc_info=True, **kwargs)

    def _log(self, level: int, message: str, **kwargs):
        extra = kwargs.pop("extra", {})
        extra.update(kwargs)
        self.logger.log(level, message, extra=extra)


# =============================================================================
# Log Context Manager
# =============================================================================

class LogContext:
    """
    Context manager for adding contextual information to logs.
    """

    _context: Dict[str, Any] = {}

    @classmethod
    def set(cls, key: str, value: Any):
        """Set a context value."""
        cls._context[key] = value

    @classmethod
    def get(cls, key: str, default: Any = None) -> Any:
        """Get a context value."""
        return cls._context.get(key, default)

    @classmethod
    def clear(cls):
        """Clear all context values."""
        cls._context.clear()

    @classmethod
    def get_all(cls) -> Dict[str, Any]:
        """Get all context values."""
        return cls._context.copy()


# =============================================================================
# Specialized Loggers
# =============================================================================

def create_trading_logger(name: str = "veloz.trading") -> StructuredLogger:
    """Create a logger optimized for trading operations."""
    return StructuredLogger(
        name=name,
        level=logging.INFO,
        service_name="veloz-gateway",
    )


def create_audit_logger(name: str = "veloz.audit") -> StructuredLogger:
    """Create a logger for audit events."""
    log_dir = os.environ.get("VELOZ_LOG_DIR", "/var/log/veloz")
    os.makedirs(log_dir, exist_ok=True)
    return StructuredLogger(
        name=name,
        level=logging.INFO,
        service_name="veloz-gateway",
        output=os.path.join(log_dir, "audit.json"),
    )


def create_error_logger(name: str = "veloz.error") -> StructuredLogger:
    """Create a logger for error tracking."""
    return StructuredLogger(
        name=name,
        level=logging.ERROR,
        service_name="veloz-gateway",
        output="stderr",
    )


# =============================================================================
# Global Logger Instance
# =============================================================================

_default_logger: Optional[StructuredLogger] = None


def get_logger(name: str = "veloz") -> StructuredLogger:
    """Get or create a structured logger."""
    global _default_logger
    if _default_logger is None:
        _default_logger = StructuredLogger(name)
    return _default_logger


def configure_logging(
    level: str = "INFO",
    service_name: str = "veloz-gateway",
    json_output: bool = True,
):
    """
    Configure the root logger for structured logging.
    Call this at application startup.
    """
    log_level = getattr(logging, level.upper(), logging.INFO)

    # Configure root logger
    root_logger = logging.getLogger()
    root_logger.setLevel(log_level)

    # Remove existing handlers
    root_logger.handlers.clear()

    if json_output:
        formatter = JSONFormatter(service_name=service_name)
    else:
        formatter = logging.Formatter(
            "%(asctime)s [%(levelname)s] %(name)s: %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S",
        )

    handler = logging.StreamHandler(sys.stdout)
    handler.setFormatter(formatter)
    root_logger.addHandler(handler)

    return root_logger


# =============================================================================
# Log Event Helpers
# =============================================================================

def log_order_event(
    logger: StructuredLogger,
    event_type: str,
    order_id: str,
    symbol: str,
    side: str,
    **kwargs
):
    """Log an order-related event with standard fields."""
    logger.info(
        f"Order {event_type}: {order_id}",
        event_type=f"order.{event_type}",
        order_id=order_id,
        symbol=symbol,
        side=side,
        component="oms",
        **kwargs
    )


def log_market_event(
    logger: StructuredLogger,
    event_type: str,
    symbol: str,
    **kwargs
):
    """Log a market data event with standard fields."""
    logger.debug(
        f"Market {event_type}: {symbol}",
        event_type=f"market.{event_type}",
        symbol=symbol,
        component="market",
        **kwargs
    )


def log_api_event(
    logger: StructuredLogger,
    method: str,
    path: str,
    status: int,
    duration_ms: float,
    **kwargs
):
    """Log an API request event with standard fields."""
    logger.info(
        f"{method} {path} -> {status}",
        event_type="api.request",
        method=method,
        path=path,
        status=status,
        duration_ms=duration_ms,
        component="gateway",
        **kwargs
    )


def log_error_event(
    logger: StructuredLogger,
    error_type: str,
    message: str,
    **kwargs
):
    """Log an error event with standard fields."""
    logger.error(
        f"Error: {message}",
        event_type=f"error.{error_type}",
        error_type=error_type,
        component=kwargs.pop("component", "unknown"),
        **kwargs
    )
