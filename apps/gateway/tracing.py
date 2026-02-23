#!/usr/bin/env python3
"""
VeloZ Gateway OpenTelemetry Tracing Module

Provides distributed tracing instrumentation using OpenTelemetry.
Supports OTLP export to collectors like Jaeger, Tempo, or Zipkin.
"""

import os
import time
import logging
from typing import Optional, Dict, Any, Callable
from contextlib import contextmanager
from functools import wraps

# Try to import OpenTelemetry, fall back to stub implementation
try:
    from opentelemetry import trace
    from opentelemetry.sdk.trace import TracerProvider
    from opentelemetry.sdk.trace.export import BatchSpanProcessor, ConsoleSpanExporter
    from opentelemetry.sdk.resources import Resource, SERVICE_NAME
    from opentelemetry.trace import Status, StatusCode, SpanKind
    from opentelemetry.trace.propagation.tracecontext import TraceContextTextMapPropagator
    from opentelemetry.propagate import set_global_textmap, get_global_textmap

    # Try to import OTLP exporter
    try:
        from opentelemetry.exporter.otlp.proto.grpc.trace_exporter import OTLPSpanExporter
        OTLP_AVAILABLE = True
    except ImportError:
        OTLP_AVAILABLE = False
        OTLPSpanExporter = None

    OTEL_AVAILABLE = True
except ImportError:
    OTEL_AVAILABLE = False
    OTLP_AVAILABLE = False

    # Stub implementations
    class StubSpan:
        def __enter__(self):
            return self
        def __exit__(self, *args):
            pass
        def set_attribute(self, key, value):
            pass
        def set_status(self, status):
            pass
        def record_exception(self, exc):
            pass
        def add_event(self, name, attributes=None):
            pass

    class StubTracer:
        def start_as_current_span(self, name, **kwargs):
            return StubSpan()
        def start_span(self, name, **kwargs):
            return StubSpan()

    class StubTracerProvider:
        def get_tracer(self, name, version=None):
            return StubTracer()
        def add_span_processor(self, processor):
            pass

    trace = type('trace', (), {
        'get_tracer': lambda name, version=None: StubTracer(),
        'set_tracer_provider': lambda provider: None,
        'get_tracer_provider': lambda: StubTracerProvider(),
    })()

    class StatusCode:
        OK = "OK"
        ERROR = "ERROR"

    class Status:
        def __init__(self, status_code, description=None):
            self.status_code = status_code
            self.description = description

    class SpanKind:
        SERVER = "SERVER"
        CLIENT = "CLIENT"
        INTERNAL = "INTERNAL"


logger = logging.getLogger("veloz.tracing")


# =============================================================================
# Tracing Configuration
# =============================================================================

class TracingConfig:
    """Configuration for OpenTelemetry tracing."""

    def __init__(
        self,
        service_name: str = "veloz-gateway",
        otlp_endpoint: Optional[str] = None,
        console_export: bool = False,
        enabled: bool = True,
        sample_rate: float = 1.0,
    ):
        self.service_name = service_name
        self.otlp_endpoint = otlp_endpoint or os.environ.get("OTEL_EXPORTER_OTLP_ENDPOINT")
        self.console_export = console_export or os.environ.get("OTEL_CONSOLE_EXPORT", "").lower() in ("true", "1")
        self.enabled = enabled and os.environ.get("OTEL_ENABLED", "true").lower() in ("true", "1")
        self.sample_rate = sample_rate


# =============================================================================
# Tracing Manager
# =============================================================================

class TracingManager:
    """Manages OpenTelemetry tracing setup and instrumentation."""

    def __init__(self, config: Optional[TracingConfig] = None):
        self.config = config or TracingConfig()
        self._tracer: Optional[Any] = None
        self._provider: Optional[Any] = None
        self._initialized = False

    def initialize(self) -> bool:
        """Initialize the tracing system."""
        if self._initialized:
            return True

        if not OTEL_AVAILABLE:
            logger.warning("OpenTelemetry not available, tracing disabled")
            return False

        if not self.config.enabled:
            logger.info("Tracing disabled by configuration")
            return False

        try:
            # Create resource with service name
            resource = Resource.create({
                SERVICE_NAME: self.config.service_name,
                "service.version": "1.0.0",
                "deployment.environment": os.environ.get("VELOZ_ENV", "development"),
            })

            # Create tracer provider
            self._provider = TracerProvider(resource=resource)

            # Add exporters
            if self.config.otlp_endpoint and OTLP_AVAILABLE:
                otlp_exporter = OTLPSpanExporter(endpoint=self.config.otlp_endpoint)
                self._provider.add_span_processor(BatchSpanProcessor(otlp_exporter))
                logger.info(f"OTLP exporter configured: {self.config.otlp_endpoint}")

            if self.config.console_export:
                console_exporter = ConsoleSpanExporter()
                self._provider.add_span_processor(BatchSpanProcessor(console_exporter))
                logger.info("Console span exporter enabled")

            # Set global tracer provider
            trace.set_tracer_provider(self._provider)

            # Set up W3C trace context propagation
            set_global_textmap(TraceContextTextMapPropagator())

            # Get tracer
            self._tracer = trace.get_tracer(self.config.service_name, "1.0.0")

            self._initialized = True
            logger.info(f"OpenTelemetry tracing initialized for {self.config.service_name}")
            return True

        except Exception as e:
            logger.error(f"Failed to initialize tracing: {e}")
            return False

    def get_tracer(self):
        """Get the tracer instance."""
        if not self._initialized:
            self.initialize()
        return self._tracer or trace.get_tracer(self.config.service_name)

    @contextmanager
    def span(self, name: str, kind: Any = None, attributes: Optional[Dict[str, Any]] = None):
        """Create a span context manager."""
        tracer = self.get_tracer()
        span_kind = kind if kind else (SpanKind.INTERNAL if OTEL_AVAILABLE else "INTERNAL")

        with tracer.start_as_current_span(name, kind=span_kind) as span:
            if attributes and OTEL_AVAILABLE:
                for key, value in attributes.items():
                    span.set_attribute(key, value)
            try:
                yield span
            except Exception as e:
                if OTEL_AVAILABLE:
                    span.set_status(Status(StatusCode.ERROR, str(e)))
                    span.record_exception(e)
                raise

    def trace_http_request(self, method: str, path: str, headers: Dict[str, str]):
        """Create a span for an HTTP request with context extraction."""
        tracer = self.get_tracer()

        # Extract trace context from headers
        if OTEL_AVAILABLE:
            propagator = get_global_textmap()
            ctx = propagator.extract(headers)
        else:
            ctx = None

        attributes = {
            "http.method": method,
            "http.url": path,
            "http.scheme": "http",
        }

        return tracer.start_as_current_span(
            f"{method} {path}",
            kind=SpanKind.SERVER if OTEL_AVAILABLE else "SERVER",
            attributes=attributes if OTEL_AVAILABLE else None,
            context=ctx,
        )

    def inject_context(self, headers: Dict[str, str]) -> Dict[str, str]:
        """Inject trace context into outgoing headers."""
        if OTEL_AVAILABLE:
            propagator = get_global_textmap()
            propagator.inject(headers)
        return headers


# =============================================================================
# Global Instance
# =============================================================================

_tracing_manager: Optional[TracingManager] = None


def get_tracing_manager() -> TracingManager:
    """Get or create the global tracing manager."""
    global _tracing_manager
    if _tracing_manager is None:
        _tracing_manager = TracingManager()
    return _tracing_manager


def init_tracing(config: Optional[TracingConfig] = None) -> TracingManager:
    """Initialize the tracing system with optional configuration."""
    global _tracing_manager
    _tracing_manager = TracingManager(config)
    _tracing_manager.initialize()
    return _tracing_manager


def tracing_available() -> bool:
    """Check if OpenTelemetry is available."""
    return OTEL_AVAILABLE


# =============================================================================
# Decorators
# =============================================================================

def traced(name: Optional[str] = None, attributes: Optional[Dict[str, Any]] = None):
    """Decorator to trace a function."""
    def decorator(func: Callable) -> Callable:
        span_name = name or func.__name__

        @wraps(func)
        def wrapper(*args, **kwargs):
            manager = get_tracing_manager()
            with manager.span(span_name, attributes=attributes):
                return func(*args, **kwargs)

        return wrapper
    return decorator


# =============================================================================
# Utility Functions
# =============================================================================

def add_span_attribute(key: str, value: Any):
    """Add an attribute to the current span."""
    if OTEL_AVAILABLE:
        span = trace.get_current_span()
        if span:
            span.set_attribute(key, value)


def add_span_event(name: str, attributes: Optional[Dict[str, Any]] = None):
    """Add an event to the current span."""
    if OTEL_AVAILABLE:
        span = trace.get_current_span()
        if span:
            span.add_event(name, attributes=attributes)


def set_span_error(error: Exception):
    """Mark the current span as errored."""
    if OTEL_AVAILABLE:
        span = trace.get_current_span()
        if span:
            span.set_status(Status(StatusCode.ERROR, str(error)))
            span.record_exception(error)
