#!/usr/bin/env python3
"""
Gateway Load Testing Script for VeloZ

This script provides comprehensive load testing for the VeloZ gateway API:
- HTTP endpoint throughput testing
- SSE stream performance testing
- Order placement latency measurement
- Concurrent connection testing

Requirements:
    pip install aiohttp asyncio statistics

Usage:
    python gateway_load_test.py --mode quick
    python gateway_load_test.py --mode full --duration 300
    python gateway_load_test.py --mode sustained --hours 24
"""

import argparse
import asyncio
import json
import logging
import random
import statistics
import sys
import time
from dataclasses import dataclass, field, asdict
from datetime import datetime
from typing import List, Dict, Optional, Callable, Any
from urllib.parse import urljoin
import tracemalloc

try:
    import aiohttp
except ImportError:
    print("Please install aiohttp: pip install aiohttp")
    sys.exit(1)

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(message)s',
    datefmt='%Y-%m-%d %H:%M:%S'
)
logger = logging.getLogger(__name__)


# ============================================================================
# Performance Targets
# ============================================================================

@dataclass
class PerformanceTargets:
    """Performance targets for gateway load testing."""
    # HTTP endpoint targets
    http_p50_ms: float = 10.0
    http_p95_ms: float = 50.0
    http_p99_ms: float = 100.0
    http_throughput_rps: float = 1000.0

    # Order endpoint targets
    order_p50_ms: float = 20.0
    order_p95_ms: float = 50.0
    order_p99_ms: float = 100.0
    order_throughput_rps: float = 100.0

    # SSE stream targets
    sse_event_latency_ms: float = 100.0
    sse_throughput_eps: float = 1000.0

    # Error rate targets
    max_error_rate: float = 0.001  # 0.1%


# ============================================================================
# Latency Tracker
# ============================================================================

class LatencyTracker:
    """Thread-safe latency tracking with percentile calculation."""

    def __init__(self, max_samples: int = 100000):
        self._samples: List[float] = []
        self._max_samples = max_samples
        self._lock = asyncio.Lock()

    async def record(self, latency_ms: float):
        async with self._lock:
            if len(self._samples) >= self._max_samples:
                # Remove oldest 10%
                self._samples = self._samples[self._max_samples // 10:]
            self._samples.append(latency_ms)

    async def get_stats(self) -> Dict[str, float]:
        async with self._lock:
            if not self._samples:
                return {
                    'count': 0,
                    'min': 0,
                    'max': 0,
                    'mean': 0,
                    'p50': 0,
                    'p95': 0,
                    'p99': 0,
                }

            sorted_samples = sorted(self._samples)
            n = len(sorted_samples)

            def percentile(p: float) -> float:
                idx = int(p * n)
                return sorted_samples[min(idx, n - 1)]

            return {
                'count': n,
                'min': sorted_samples[0],
                'max': sorted_samples[-1],
                'mean': statistics.mean(sorted_samples),
                'p50': percentile(0.50),
                'p95': percentile(0.95),
                'p99': percentile(0.99),
            }

    async def reset(self):
        async with self._lock:
            self._samples.clear()


# ============================================================================
# Load Test Result
# ============================================================================

@dataclass
class LoadTestResult:
    """Result of a load test run."""
    test_name: str
    passed: bool = False

    # Throughput metrics
    requests_per_sec: float = 0.0
    total_requests: int = 0

    # Latency metrics (milliseconds)
    latency_p50_ms: float = 0.0
    latency_p95_ms: float = 0.0
    latency_p99_ms: float = 0.0
    latency_mean_ms: float = 0.0
    latency_min_ms: float = 0.0
    latency_max_ms: float = 0.0

    # Error metrics
    errors: int = 0
    error_rate: float = 0.0

    # Duration
    duration_sec: float = 0.0

    # Memory metrics
    memory_start_mb: float = 0.0
    memory_end_mb: float = 0.0
    memory_growth_pct: float = 0.0

    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)

    def __str__(self) -> str:
        return f"""
================================================================================
Load Test: {self.test_name}
================================================================================
Status: {'PASSED' if self.passed else 'FAILED'}

Throughput:
  Requests/sec: {self.requests_per_sec:.2f}
  Total Requests: {self.total_requests}

Latency (milliseconds):
  P50:  {self.latency_p50_ms:.2f}
  P95:  {self.latency_p95_ms:.2f}
  P99:  {self.latency_p99_ms:.2f}
  Mean: {self.latency_mean_ms:.2f}
  Min:  {self.latency_min_ms:.2f}
  Max:  {self.latency_max_ms:.2f}

Errors:
  Count: {self.errors}
  Rate:  {self.error_rate * 100:.3f}%

Duration: {self.duration_sec:.2f} seconds
"""


# ============================================================================
# Gateway Load Tester
# ============================================================================

class GatewayLoadTester:
    """Load tester for VeloZ gateway API."""

    def __init__(
        self,
        base_url: str = "http://127.0.0.1:8080",
        targets: Optional[PerformanceTargets] = None
    ):
        self.base_url = base_url.rstrip('/')
        self.targets = targets or PerformanceTargets()
        self._session: Optional[aiohttp.ClientSession] = None

    async def __aenter__(self):
        self._session = aiohttp.ClientSession(
            timeout=aiohttp.ClientTimeout(total=30)
        )
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self._session:
            await self._session.close()

    async def _make_request(
        self,
        method: str,
        endpoint: str,
        data: Optional[Dict] = None,
        headers: Optional[Dict] = None
    ) -> tuple[int, float, Optional[Dict]]:
        """Make HTTP request and return (status, latency_ms, response_data)."""
        url = urljoin(self.base_url, endpoint)
        start = time.perf_counter()

        try:
            async with self._session.request(
                method, url, json=data, headers=headers
            ) as response:
                latency_ms = (time.perf_counter() - start) * 1000
                try:
                    response_data = await response.json()
                except:
                    response_data = None
                return response.status, latency_ms, response_data
        except Exception as e:
            latency_ms = (time.perf_counter() - start) * 1000
            logger.debug(f"Request failed: {e}")
            return 0, latency_ms, None

    async def run_http_throughput_test(
        self,
        duration_sec: int = 60,
        concurrency: int = 10,
        endpoint: str = "/api/status"
    ) -> LoadTestResult:
        """Test HTTP endpoint throughput."""
        result = LoadTestResult(
            test_name=f"HTTP Throughput ({endpoint}, {concurrency} concurrent)"
        )

        latency_tracker = LatencyTracker()
        request_count = 0
        error_count = 0
        running = True

        async def worker():
            nonlocal request_count, error_count
            while running:
                status, latency_ms, _ = await self._make_request("GET", endpoint)
                await latency_tracker.record(latency_ms)
                request_count += 1
                if status != 200:
                    error_count += 1

        # Start workers
        start_time = time.perf_counter()
        tasks = [asyncio.create_task(worker()) for _ in range(concurrency)]

        # Run for duration
        await asyncio.sleep(duration_sec)
        running = False

        # Wait for workers to finish
        for task in tasks:
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass

        end_time = time.perf_counter()
        result.duration_sec = end_time - start_time

        # Collect stats
        stats = await latency_tracker.get_stats()
        result.total_requests = request_count
        result.requests_per_sec = request_count / result.duration_sec
        result.latency_p50_ms = stats['p50']
        result.latency_p95_ms = stats['p95']
        result.latency_p99_ms = stats['p99']
        result.latency_mean_ms = stats['mean']
        result.latency_min_ms = stats['min']
        result.latency_max_ms = stats['max']
        result.errors = error_count
        result.error_rate = error_count / request_count if request_count > 0 else 0

        # Validate against targets
        result.passed = (
            result.latency_p99_ms <= self.targets.http_p99_ms and
            result.requests_per_sec >= self.targets.http_throughput_rps * 0.9 and
            result.error_rate <= self.targets.max_error_rate
        )

        return result

    async def run_order_latency_test(
        self,
        duration_sec: int = 60,
        orders_per_sec: float = 10.0
    ) -> LoadTestResult:
        """Test order placement latency."""
        result = LoadTestResult(
            test_name=f"Order Latency ({orders_per_sec} orders/sec)"
        )

        latency_tracker = LatencyTracker()
        request_count = 0
        error_count = 0

        symbols = [f"SYM{i}USDT" for i in range(100)]
        order_id = 0

        start_time = time.perf_counter()
        end_time = start_time + duration_sec
        interval = 1.0 / orders_per_sec

        while time.perf_counter() < end_time:
            order_id += 1
            order_data = {
                "client_order_id": f"LOAD_{order_id}",
                "symbol": random.choice(symbols),
                "side": random.choice(["buy", "sell"]),
                "type": "limit",
                "qty": round(random.uniform(0.001, 1.0), 8),
                "price": round(random.uniform(40000, 60000), 2)
            }

            status, latency_ms, _ = await self._make_request(
                "POST", "/api/order", data=order_data
            )
            await latency_tracker.record(latency_ms)
            request_count += 1
            if status not in (200, 201):
                error_count += 1

            # Rate limiting
            await asyncio.sleep(interval)

        result.duration_sec = time.perf_counter() - start_time

        # Collect stats
        stats = await latency_tracker.get_stats()
        result.total_requests = request_count
        result.requests_per_sec = request_count / result.duration_sec
        result.latency_p50_ms = stats['p50']
        result.latency_p95_ms = stats['p95']
        result.latency_p99_ms = stats['p99']
        result.latency_mean_ms = stats['mean']
        result.latency_min_ms = stats['min']
        result.latency_max_ms = stats['max']
        result.errors = error_count
        result.error_rate = error_count / request_count if request_count > 0 else 0

        # Validate against targets
        result.passed = (
            result.latency_p99_ms <= self.targets.order_p99_ms and
            result.error_rate <= self.targets.max_error_rate
        )

        return result

    async def run_sse_stream_test(
        self,
        duration_sec: int = 60
    ) -> LoadTestResult:
        """Test SSE stream performance."""
        result = LoadTestResult(test_name="SSE Stream Performance")

        event_count = 0
        latencies: List[float] = []
        error_count = 0

        start_time = time.perf_counter()

        try:
            url = urljoin(self.base_url, "/api/stream")
            async with self._session.get(url) as response:
                if response.status != 200:
                    result.errors = 1
                    result.error_rate = 1.0
                    return result

                async for line in response.content:
                    if time.perf_counter() - start_time > duration_sec:
                        break

                    line = line.decode('utf-8').strip()
                    if line.startswith('data:'):
                        recv_time = time.perf_counter()
                        try:
                            data = json.loads(line[5:])
                            if 'ts_pub_ns' in data:
                                # Calculate latency from publish time
                                pub_time_sec = data['ts_pub_ns'] / 1e9
                                latency_ms = (recv_time - pub_time_sec) * 1000
                                if 0 < latency_ms < 10000:  # Sanity check
                                    latencies.append(latency_ms)
                            event_count += 1
                        except json.JSONDecodeError:
                            error_count += 1

        except Exception as e:
            logger.error(f"SSE stream error: {e}")
            error_count += 1

        result.duration_sec = time.perf_counter() - start_time
        result.total_requests = event_count
        result.requests_per_sec = event_count / result.duration_sec if result.duration_sec > 0 else 0
        result.errors = error_count
        result.error_rate = error_count / event_count if event_count > 0 else 0

        if latencies:
            latencies.sort()
            n = len(latencies)
            result.latency_min_ms = latencies[0]
            result.latency_max_ms = latencies[-1]
            result.latency_mean_ms = statistics.mean(latencies)
            result.latency_p50_ms = latencies[int(0.50 * n)]
            result.latency_p95_ms = latencies[int(0.95 * n)]
            result.latency_p99_ms = latencies[min(int(0.99 * n), n - 1)]

        # Validate against targets
        result.passed = (
            result.latency_p99_ms <= self.targets.sse_event_latency_ms and
            result.requests_per_sec >= self.targets.sse_throughput_eps * 0.5 and
            result.error_rate <= self.targets.max_error_rate
        )

        return result

    async def run_concurrent_connections_test(
        self,
        num_connections: int = 100,
        duration_sec: int = 60
    ) -> LoadTestResult:
        """Test handling of many concurrent connections."""
        result = LoadTestResult(
            test_name=f"Concurrent Connections ({num_connections} connections)"
        )

        latency_tracker = LatencyTracker()
        request_count = 0
        error_count = 0
        running = True

        async def connection_worker(worker_id: int):
            nonlocal request_count, error_count
            # Create separate session for each worker
            async with aiohttp.ClientSession(
                timeout=aiohttp.ClientTimeout(total=30)
            ) as session:
                while running:
                    try:
                        url = urljoin(self.base_url, "/api/status")
                        start = time.perf_counter()
                        async with session.get(url) as response:
                            latency_ms = (time.perf_counter() - start) * 1000
                            await latency_tracker.record(latency_ms)
                            request_count += 1
                            if response.status != 200:
                                error_count += 1
                    except Exception:
                        error_count += 1
                    await asyncio.sleep(0.1)  # Small delay between requests

        start_time = time.perf_counter()
        tasks = [
            asyncio.create_task(connection_worker(i))
            for i in range(num_connections)
        ]

        await asyncio.sleep(duration_sec)
        running = False

        for task in tasks:
            task.cancel()
            try:
                await task
            except asyncio.CancelledError:
                pass

        result.duration_sec = time.perf_counter() - start_time

        # Collect stats
        stats = await latency_tracker.get_stats()
        result.total_requests = request_count
        result.requests_per_sec = request_count / result.duration_sec
        result.latency_p50_ms = stats['p50']
        result.latency_p95_ms = stats['p95']
        result.latency_p99_ms = stats['p99']
        result.latency_mean_ms = stats['mean']
        result.latency_min_ms = stats['min']
        result.latency_max_ms = stats['max']
        result.errors = error_count
        result.error_rate = error_count / request_count if request_count > 0 else 0

        # Validate - more lenient for concurrent test
        result.passed = (
            result.latency_p99_ms <= self.targets.http_p99_ms * 2 and
            result.error_rate <= self.targets.max_error_rate * 10
        )

        return result


# ============================================================================
# Load Test Suite
# ============================================================================

class LoadTestSuite:
    """Collection of load test results."""

    def __init__(self, name: str):
        self.name = name
        self.results: List[LoadTestResult] = []

    def add_result(self, result: LoadTestResult):
        self.results.append(result)

    def all_passed(self) -> bool:
        return all(r.passed for r in self.results)

    def generate_report(self) -> str:
        lines = [
            "",
            "################################################################################",
            "#                         GATEWAY LOAD TEST REPORT                            #",
            "################################################################################",
            "",
            f"Suite: {self.name}",
            f"Status: {'ALL TESTS PASSED' if self.all_passed() else 'SOME TESTS FAILED'}",
            "",
        ]

        for result in self.results:
            lines.append(str(result))

        # Summary table
        lines.extend([
            "################################################################################",
            "#                              SUMMARY                                         #",
            "################################################################################",
            "",
            "Test Name                                    | Status | RPS      | P99 (ms)",
            "--------------------------------------------|--------|----------|----------",
        ])

        for result in self.results:
            status = "PASS" if result.passed else "FAIL"
            lines.append(
                f"{result.test_name[:44]:<44} | {status:<6} | "
                f"{result.requests_per_sec:>8.1f} | {result.latency_p99_ms:>8.2f}"
            )

        return "\n".join(lines)

    def to_json(self) -> str:
        return json.dumps({
            "suite": self.name,
            "all_passed": self.all_passed(),
            "results": [r.to_dict() for r in self.results]
        }, indent=2)


# ============================================================================
# Test Scenarios
# ============================================================================

async def run_quick_test(base_url: str):
    """Run quick load test (30 seconds per test)."""
    logger.info("Running quick gateway load test...")

    suite = LoadTestSuite("Quick Gateway Load Test")

    async with GatewayLoadTester(base_url) as tester:
        # HTTP throughput test
        logger.info("Testing HTTP throughput...")
        result = await tester.run_http_throughput_test(
            duration_sec=30,
            concurrency=5
        )
        suite.add_result(result)

        # Order latency test
        logger.info("Testing order latency...")
        result = await tester.run_order_latency_test(
            duration_sec=30,
            orders_per_sec=5.0
        )
        suite.add_result(result)

    print(suite.generate_report())
    return suite.all_passed()


async def run_full_test(base_url: str, duration_sec: int = 300):
    """Run full load test."""
    logger.info(f"Running full gateway load test ({duration_sec}s per test)...")

    suite = LoadTestSuite("Full Gateway Load Test")

    async with GatewayLoadTester(base_url) as tester:
        # HTTP throughput test
        logger.info("Testing HTTP throughput...")
        result = await tester.run_http_throughput_test(
            duration_sec=duration_sec,
            concurrency=20
        )
        suite.add_result(result)

        # Order latency test
        logger.info("Testing order latency...")
        result = await tester.run_order_latency_test(
            duration_sec=duration_sec,
            orders_per_sec=50.0
        )
        suite.add_result(result)

        # SSE stream test
        logger.info("Testing SSE stream...")
        result = await tester.run_sse_stream_test(duration_sec=duration_sec)
        suite.add_result(result)

        # Concurrent connections test
        logger.info("Testing concurrent connections...")
        result = await tester.run_concurrent_connections_test(
            num_connections=50,
            duration_sec=duration_sec
        )
        suite.add_result(result)

    print(suite.generate_report())
    print("\nJSON Report:")
    print(suite.to_json())

    return suite.all_passed()


async def run_sustained_test(base_url: str, hours: int = 1):
    """Run sustained load test for memory leak detection."""
    logger.info(f"Running sustained gateway load test ({hours} hours)...")

    # Start memory tracking
    tracemalloc.start()
    start_snapshot = tracemalloc.take_snapshot()

    suite = LoadTestSuite(f"Sustained Gateway Load Test ({hours}h)")

    duration_sec = hours * 3600
    report_interval = 300  # Report every 5 minutes

    async with GatewayLoadTester(base_url) as tester:
        start_time = time.perf_counter()
        end_time = start_time + duration_sec

        iteration = 0
        while time.perf_counter() < end_time:
            iteration += 1
            remaining = end_time - time.perf_counter()
            test_duration = min(report_interval, remaining)

            if test_duration <= 0:
                break

            logger.info(f"Iteration {iteration}, {remaining/3600:.2f} hours remaining...")

            # Run mixed workload
            result = await tester.run_http_throughput_test(
                duration_sec=int(test_duration),
                concurrency=10
            )
            result.test_name = f"Iteration {iteration}"
            suite.add_result(result)

            # Check memory
            current_snapshot = tracemalloc.take_snapshot()
            top_stats = current_snapshot.compare_to(start_snapshot, 'lineno')
            total_diff = sum(stat.size_diff for stat in top_stats)
            logger.info(f"Memory change: {total_diff / 1024 / 1024:.2f} MB")

    # Final memory check
    end_snapshot = tracemalloc.take_snapshot()
    top_stats = end_snapshot.compare_to(start_snapshot, 'lineno')
    total_diff = sum(stat.size_diff for stat in top_stats)

    print(suite.generate_report())
    print(f"\nTotal memory growth: {total_diff / 1024 / 1024:.2f} MB")

    tracemalloc.stop()

    # Check for memory leak (> 100MB growth is suspicious)
    memory_ok = total_diff < 100 * 1024 * 1024
    return suite.all_passed() and memory_ok


async def run_stress_test(base_url: str):
    """Run stress test to find breaking point."""
    logger.info("Running gateway stress test...")

    suite = LoadTestSuite("Gateway Stress Test")

    # Progressively increase load
    concurrency_levels = [10, 25, 50, 100, 200]

    async with GatewayLoadTester(base_url) as tester:
        for concurrency in concurrency_levels:
            logger.info(f"Testing with {concurrency} concurrent connections...")

            result = await tester.run_http_throughput_test(
                duration_sec=30,
                concurrency=concurrency
            )
            result.test_name = f"Stress Test ({concurrency} concurrent)"
            suite.add_result(result)

            # Stop if error rate exceeds 10%
            if result.error_rate > 0.1:
                logger.warning(f"Error rate exceeded 10% at {concurrency} connections")
                break

    print(suite.generate_report())
    return True  # Stress test is informational


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="VeloZ Gateway Load Testing"
    )
    parser.add_argument(
        "--mode",
        choices=["quick", "full", "sustained", "stress"],
        default="quick",
        help="Test mode"
    )
    parser.add_argument(
        "--url",
        default="http://127.0.0.1:8080",
        help="Gateway base URL"
    )
    parser.add_argument(
        "--duration",
        type=int,
        default=300,
        help="Duration in seconds for full test"
    )
    parser.add_argument(
        "--hours",
        type=int,
        default=1,
        help="Duration in hours for sustained test"
    )

    args = parser.parse_args()

    if args.mode == "quick":
        success = asyncio.run(run_quick_test(args.url))
    elif args.mode == "full":
        success = asyncio.run(run_full_test(args.url, args.duration))
    elif args.mode == "sustained":
        success = asyncio.run(run_sustained_test(args.url, args.hours))
    elif args.mode == "stress":
        success = asyncio.run(run_stress_test(args.url))
    else:
        parser.print_help()
        sys.exit(1)

    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
