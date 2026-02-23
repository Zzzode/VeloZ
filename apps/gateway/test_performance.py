#!/usr/bin/env python3
"""Performance regression tests for the gateway.

Tests performance characteristics to detect regressions:
- Request latency
- Throughput under load
- Memory usage
- Connection handling
- Rate limiter performance
"""

import gc
import os
import statistics
import sys
import threading
import time
import tracemalloc
import unittest
from concurrent.futures import ThreadPoolExecutor, as_completed
from unittest.mock import MagicMock, patch

# Add gateway directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gateway import (
    JWTManager,
    APIKeyManager,
    RateLimiter,
    OrderStore,
)


class TestJWTPerformance(unittest.TestCase):
    """Performance tests for JWT operations."""

    def setUp(self):
        self.jwt = JWTManager("test_secret_key_12345", token_expiry_seconds=3600)

    def tearDown(self):
        self.jwt.stop()

    def test_token_creation_latency(self):
        """Test JWT token creation latency."""
        iterations = 1000
        latencies = []

        for _ in range(iterations):
            start = time.perf_counter()
            self.jwt.create_token("user123")
            latencies.append((time.perf_counter() - start) * 1000)  # ms

        avg_latency = statistics.mean(latencies)
        p99_latency = statistics.quantiles(latencies, n=100)[98]

        # Performance assertions
        self.assertLess(avg_latency, 1.0, "Average token creation should be < 1ms")
        self.assertLess(p99_latency, 5.0, "P99 token creation should be < 5ms")

    def test_token_verification_latency(self):
        """Test JWT token verification latency."""
        token = self.jwt.create_token("user123")
        iterations = 1000
        latencies = []

        for _ in range(iterations):
            start = time.perf_counter()
            self.jwt.verify_token(token)
            latencies.append((time.perf_counter() - start) * 1000)  # ms

        avg_latency = statistics.mean(latencies)
        p99_latency = statistics.quantiles(latencies, n=100)[98]

        # Performance assertions
        self.assertLess(avg_latency, 0.5, "Average token verification should be < 0.5ms")
        self.assertLess(p99_latency, 2.0, "P99 token verification should be < 2ms")

    def test_concurrent_token_operations(self):
        """Test concurrent JWT operations throughput."""
        iterations = 100
        threads = 10
        results = []

        def create_and_verify():
            latencies = []
            for _ in range(iterations):
                start = time.perf_counter()
                token = self.jwt.create_token("user123")
                self.jwt.verify_token(token)
                latencies.append((time.perf_counter() - start) * 1000)
            return latencies

        with ThreadPoolExecutor(max_workers=threads) as executor:
            futures = [executor.submit(create_and_verify) for _ in range(threads)]
            for future in as_completed(futures):
                results.extend(future.result())

        avg_latency = statistics.mean(results)
        # Under concurrent load, latency may be higher
        self.assertLess(avg_latency, 5.0, "Average concurrent operation should be < 5ms")


class TestAPIKeyPerformance(unittest.TestCase):
    """Performance tests for API key operations."""

    def setUp(self):
        self.manager = APIKeyManager()

    def test_key_validation_latency(self):
        """Test API key validation latency."""
        key_id, raw_key = self.manager.create_key("user123", "test-key")
        iterations = 1000
        latencies = []

        for _ in range(iterations):
            start = time.perf_counter()
            self.manager.validate_key(raw_key)
            latencies.append((time.perf_counter() - start) * 1000)  # ms

        avg_latency = statistics.mean(latencies)
        p99_latency = statistics.quantiles(latencies, n=100)[98]

        # Performance assertions
        self.assertLess(avg_latency, 0.5, "Average key validation should be < 0.5ms")
        self.assertLess(p99_latency, 2.0, "P99 key validation should be < 2ms")

    def test_many_keys_performance(self):
        """Test performance with many API keys."""
        # Create many keys
        keys = []
        for i in range(100):
            key_id, raw_key = self.manager.create_key(f"user{i}", f"key{i}")
            keys.append(raw_key)

        # Validate random keys
        iterations = 1000
        latencies = []

        for i in range(iterations):
            key = keys[i % len(keys)]
            start = time.perf_counter()
            self.manager.validate_key(key)
            latencies.append((time.perf_counter() - start) * 1000)

        avg_latency = statistics.mean(latencies)
        # Should still be fast with many keys
        self.assertLess(avg_latency, 1.0, "Average validation with 100 keys should be < 1ms")


class TestRateLimiterPerformance(unittest.TestCase):
    """Performance tests for rate limiter."""

    def test_rate_limit_check_latency(self):
        """Test rate limit check latency."""
        limiter = RateLimiter(capacity=1000, refill_rate=100.0)
        iterations = 10000
        latencies = []

        for i in range(iterations):
            start = time.perf_counter()
            limiter.check_rate_limit(f"user{i % 100}")
            latencies.append((time.perf_counter() - start) * 1000)  # ms

        avg_latency = statistics.mean(latencies)
        p99_latency = statistics.quantiles(latencies, n=100)[98]

        # Rate limiting should be very fast
        self.assertLess(avg_latency, 0.1, "Average rate limit check should be < 0.1ms")
        self.assertLess(p99_latency, 0.5, "P99 rate limit check should be < 0.5ms")

    def test_concurrent_rate_limiting(self):
        """Test rate limiter under concurrent load."""
        limiter = RateLimiter(capacity=1000, refill_rate=100.0)
        threads = 10
        iterations = 1000
        results = []

        def check_rate_limit():
            latencies = []
            for i in range(iterations):
                start = time.perf_counter()
                limiter.check_rate_limit(f"user{i % 10}")
                latencies.append((time.perf_counter() - start) * 1000)
            return latencies

        with ThreadPoolExecutor(max_workers=threads) as executor:
            futures = [executor.submit(check_rate_limit) for _ in range(threads)]
            for future in as_completed(futures):
                results.extend(future.result())

        avg_latency = statistics.mean(results)
        # Under concurrent load, should still be fast
        self.assertLess(avg_latency, 0.5, "Average concurrent rate limit should be < 0.5ms")


class TestOrderStorePerformance(unittest.TestCase):
    """Performance tests for order store."""

    def setUp(self):
        self.store = OrderStore()

    def test_order_add_latency(self):
        """Test order add latency."""
        iterations = 1000
        latencies = []

        for i in range(iterations):
            start = time.perf_counter()
            self.store.note_order_params(
                client_order_id=f"order{i}",
                symbol="BTCUSDT",
                side="BUY",
                qty=0.1,
                price=50000.0,
            )
            latencies.append((time.perf_counter() - start) * 1000)

        avg_latency = statistics.mean(latencies)
        p99_latency = statistics.quantiles(latencies, n=100)[98]

        self.assertLess(avg_latency, 0.5, "Average order add should be < 0.5ms")
        self.assertLess(p99_latency, 2.0, "P99 order add should be < 2ms")

    def test_order_lookup_latency(self):
        """Test order lookup latency."""
        # Add orders first
        for i in range(1000):
            self.store.note_order_params(
                client_order_id=f"order{i}",
                symbol="BTCUSDT",
                side="BUY",
                qty=0.1,
                price=50000.0,
            )

        iterations = 1000
        latencies = []

        for i in range(iterations):
            start = time.perf_counter()
            self.store.get(f"order{i % 1000}")  # Get single order by ID
            latencies.append((time.perf_counter() - start) * 1000)

        avg_latency = statistics.mean(latencies)
        p99_latency = statistics.quantiles(latencies, n=100)[98]

        self.assertLess(avg_latency, 0.1, "Average order lookup should be < 0.1ms")
        self.assertLess(p99_latency, 0.5, "P99 order lookup should be < 0.5ms")


class TestMemoryUsage(unittest.TestCase):
    """Tests for memory usage patterns."""

    def test_jwt_memory_usage(self):
        """Test JWT operations don't leak memory."""
        jwt = JWTManager("test_secret_key_12345", token_expiry_seconds=3600)

        # Force garbage collection
        gc.collect()
        tracemalloc.start()

        # Create many tokens
        for _ in range(10000):
            token = jwt.create_token("user123")
            jwt.verify_token(token)

        current, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        jwt.stop()

        # Memory usage should be reasonable (< 50MB for 10k operations)
        self.assertLess(peak / 1024 / 1024, 50, "Peak memory should be < 50MB")

    def test_rate_limiter_memory_usage(self):
        """Test rate limiter doesn't leak memory with many users."""
        limiter = RateLimiter(capacity=100, refill_rate=10.0)

        gc.collect()
        tracemalloc.start()

        # Simulate many unique users
        for i in range(10000):
            limiter.check_rate_limit(f"user{i}")

        current, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()

        # Memory usage should be reasonable
        self.assertLess(peak / 1024 / 1024, 50, "Peak memory should be < 50MB")


class TestThroughput(unittest.TestCase):
    """Tests for throughput under load."""

    def test_jwt_throughput(self):
        """Test JWT operations throughput."""
        jwt = JWTManager("test_secret_key_12345", token_expiry_seconds=3600)

        iterations = 10000
        start = time.perf_counter()

        for _ in range(iterations):
            token = jwt.create_token("user123")
            jwt.verify_token(token)

        elapsed = time.perf_counter() - start
        ops_per_second = iterations / elapsed

        jwt.stop()

        # Should handle at least 1000 ops/second
        self.assertGreater(ops_per_second, 1000, "JWT throughput should be > 1000 ops/sec")

    def test_rate_limiter_throughput(self):
        """Test rate limiter throughput."""
        limiter = RateLimiter(capacity=10000, refill_rate=1000.0)

        iterations = 100000
        start = time.perf_counter()

        for i in range(iterations):
            limiter.check_rate_limit(f"user{i % 100}")

        elapsed = time.perf_counter() - start
        ops_per_second = iterations / elapsed

        # Rate limiter should be very fast
        self.assertGreater(ops_per_second, 100000, "Rate limiter should handle > 100k ops/sec")


class TestLatencyDistribution(unittest.TestCase):
    """Tests for latency distribution characteristics."""

    def test_jwt_latency_distribution(self):
        """Test JWT latency distribution is consistent."""
        jwt = JWTManager("test_secret_key_12345", token_expiry_seconds=3600)

        iterations = 1000
        latencies = []

        for _ in range(iterations):
            start = time.perf_counter()
            token = jwt.create_token("user123")
            jwt.verify_token(token)
            latencies.append((time.perf_counter() - start) * 1000)

        jwt.stop()

        avg = statistics.mean(latencies)
        stdev = statistics.stdev(latencies)
        p50 = statistics.median(latencies)
        p99 = statistics.quantiles(latencies, n=100)[98]

        # Latency should be consistent (low standard deviation)
        self.assertLess(stdev / avg, 2.0, "Latency standard deviation should be < 2x average")

        # P99 should not be too far from P50
        self.assertLess(p99 / p50, 10.0, "P99 should be < 10x P50")


if __name__ == "__main__":
    unittest.main(verbosity=2)
