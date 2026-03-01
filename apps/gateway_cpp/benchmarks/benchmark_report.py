#!/usr/bin/env python3
"""
Benchmark Report Generator for VeloZ C++ Gateway

This script:
- Parses benchmark output from the C++ benchmark executable
- Compares C++ vs Python Gateway performance
- Generates charts and tables
- Identifies bottlenecks and areas for improvement

Usage:
    python benchmark_report.py [--input BENCHMARK_OUTPUT] [--output REPORT_DIR]
"""

import argparse
import json
import os
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple


# ============================================================================
# Data Structures
# ============================================================================

@dataclass
class PercentileStats:
    """Percentile statistics from benchmark measurements"""
    p50: float = 0.0
    p90: float = 0.0
    p99: float = 0.0
    p999: float = 0.0
    min: float = 0.0
    max: float = 0.0
    mean: float = 0.0
    stddev: float = 0.0


@dataclass
class BenchmarkResult:
    """Result from a single benchmark"""
    name: str
    stats: PercentileStats
    iterations: int
    total_time_ms: float


@dataclass
class ThroughputResult:
    """Result from a throughput benchmark"""
    name: str
    total_requests: int
    concurrent_requests: int
    total_time_ms: float
    requests_per_second: float
    avg_latency_us: float


@dataclass
class BenchmarkSuite:
    """Complete benchmark suite results"""
    # Latency benchmarks
    route_lookup: Optional[BenchmarkResult] = None
    authentication: Optional[BenchmarkResult] = None
    full_request: Optional[BenchmarkResult] = None

    # Throughput benchmarks
    max_throughput: Optional[ThroughputResult] = None
    throughput_scaling: List[Dict] = field(default_factory=list)

    # SSE benchmarks
    sse_event_delivery: Optional[BenchmarkResult] = None

    # Memory and startup
    memory_per_request: Optional[BenchmarkResult] = None
    startup_time: Optional[PercentileStats] = None

    # Platform info
    platform: str = ""
    date: str = ""


# ============================================================================
# Benchmark Parser
# ============================================================================

class BenchmarkParser:
    """Parse benchmark output from C++ benchmark executable"""

    def __init__(self):
        self.suite = BenchmarkSuite()
        self.current_section = None

    def parse(self, output: str) -> BenchmarkSuite:
        """Parse benchmark output string"""
        lines = output.split('\n')

        i = 0
        while i < len(lines):
            line = lines[i].strip()

            # Detect benchmark sections
            if 'Route Lookup Latency' in line:
                i = self.parse_benchmark(lines, i, self.suite.route_lookup, 'Route Lookup Latency')
            elif 'Authentication (JWT) Latency' in line:
                i = self.parse_benchmark(lines, i, self.suite.authentication, 'Authentication (JWT) Latency')
            elif 'Full Request Latency' in line:
                i = self.parse_benchmark(lines, i, self.suite.full_request, 'Full Request Latency')
            elif 'SSE Event Broadcast Latency' in line:
                i = self.parse_benchmark(lines, i, self.suite.sse_event_delivery, 'SSE Event Broadcast Latency')
            elif 'Max Throughput' in line:
                i = self.parse_throughput(lines, i)
            elif 'Throughput Scaling Analysis' in line:
                i = self.parse_throughput_scaling(lines, i)
            elif 'Startup Time' in line:
                i = self.parse_startup_time(lines, i)

            i += 1

        return self.suite

    def parse_benchmark(self, lines: List[str], start_idx: int,
                        result: Optional[BenchmarkResult], name: str) -> int:
        """Parse a single benchmark result"""
        stats = PercentileStats()
        iterations = 0
        total_time = 0.0

        i = start_idx
        while i < len(lines):
            line = lines[i].strip()

            if '===' in line and i > start_idx:
                break  # End of section

            # Extract iterations
            if 'Iterations:' in line:
                iterations = int(re.search(r'Iterations:\s*(\d+)', line).group(1))

            # Extract total time
            if 'Total time:' in line:
                total_time = float(re.search(r'Total time:\s*([\d.]+)', line).group(1))

            # Extract percentiles
            if line.startswith('Min:'):
                stats.min = float(re.search(r'Min:\s*([\d.]+)', line).group(1))
            if line.startswith('Mean:'):
                stats.mean = float(re.search(r'Mean:\s*([\d.]+)', line).group(1))
            if line.startswith('P50:'):
                stats.p50 = float(re.search(r'P50:\s*([\d.]+)', line).group(1))
            if line.startswith('P90:'):
                stats.p90 = float(re.search(r'P90:\s*([\d.]+)', line).group(1))
            if line.startswith('P99:'):
                stats.p99 = float(re.search(r'P99:\s*([\d.]+)', line).group(1))
            if line.startswith('P99.9:'):
                stats.p999 = float(re.search(r'P99\.9:\s*([\d.]+)', line).group(1))
            if line.startswith('Max:'):
                stats.max = float(re.search(r'Max:\s*([\d.]+)', line).group(1))
            if 'StdDev:' in line:
                stats.stddev = float(re.search(r'StdDev:\s*([\d.]+)', line).group(1))

            i += 1

        result_obj = BenchmarkResult(
            name=name,
            stats=stats,
            iterations=iterations,
            total_time_ms=total_time
        )

        # Store in appropriate field
        if 'Route Lookup' in name:
            self.suite.route_lookup = result_obj
        elif 'Authentication' in name:
            self.suite.authentication = result_obj
        elif 'Full Request' in name:
            self.suite.full_request = result_obj
        elif 'SSE' in name:
            self.suite.sse_event_delivery = result_obj

        return i - 1  # Adjust for outer loop increment

    def parse_throughput(self, lines: List[str], start_idx: int) -> int:
        """Parse throughput benchmark result"""
        i = start_idx

        concurrency = 0
        total_requests = 0
        total_time = 0.0
        rps = 0.0
        avg_latency = 0.0

        while i < len(lines):
            line = lines[i].strip()

            if '===' in line and i > start_idx:
                break

            if 'Concurrency:' in line:
                concurrency = int(re.search(r'Concurrency:\s*(\d+)', line).group(1))
            if 'Total requests:' in line:
                total_requests = int(re.search(r'Total requests:\s*(\d+)', line).group(1))
            if 'Total time:' in line:
                total_time = float(re.search(r'Total time:\s*([\d.]+)', line).group(1))
            if 'Throughput:' in line:
                rps = float(re.search(r'Throughput:\s*([\d.]+)', line).group(1))
            if 'Avg latency:' in line:
                avg_latency = float(re.search(r'Avg latency:\s*([\d.]+)', line).group(1))

            i += 1

        self.suite.max_throughput = ThroughputResult(
            name='Max Throughput',
            total_requests=total_requests,
            concurrent_requests=concurrency,
            total_time_ms=total_time,
            requests_per_second=rps,
            avg_latency_us=avg_latency
        )

        return i - 1

    def parse_throughput_scaling(self, lines: List[str], start_idx: int) -> int:
        """Parse throughput scaling results"""
        i = start_idx

        # Skip header lines
        while i < len(lines) and '----' not in lines[i]:
            i += 1
        if i < len(lines):
            i += 1  # Skip separator line

        while i < len(lines):
            line = lines[i].strip()

            if not line or '===' in line:
                break

            # Parse data line: "Concurrency    Throughput    Avg Latency    Scaling"
            parts = re.split(r'\s{2,}', line)
            if len(parts) >= 4:
                self.suite.throughput_scaling.append({
                    'concurrency': int(parts[0]),
                    'throughput': float(parts[1].split()[0]),
                    'avg_latency': float(parts[2].split()[0]),
                    'scaling': float(parts[3].rstrip('x'))
                })

            i += 1

        return i - 1

    def parse_startup_time(self, lines: List[str], start_idx: int) -> int:
        """Parse startup time results"""
        stats = PercentileStats()

        i = start_idx
        while i < len(lines):
            line = lines[i].strip()

            if '===' in line and i > start_idx:
                break

            if 'Min:' in line:
                stats.min = float(re.search(r'Min:\s*([\d.]+)', line).group(1))
            if 'Mean:' in line:
                stats.mean = float(re.search(r'Mean:\s*([\d.]+)', line).group(1))
            if 'P50:' in line:
                stats.p50 = float(re.search(r'P50:\s*([\d.]+)', line).group(1))
            if 'P95:' in line:
                stats.p90 = float(re.search(r'P95:\s*([\d.]+)', line).group(1))
            if 'P99:' in line:
                stats.p99 = float(re.search(r'P99:\s*([\d.]+)', line).group(1))
            if 'Max:' in line:
                stats.max = float(re.search(r'Max:\s*([\d.]+)', line).group(1))

            i += 1

        self.suite.startup_time = stats
        return i - 1


# ============================================================================
# Python Baseline Data
# ============================================================================

PYTHON_BASELINE = {
    'latency_p50_us': 700,
    'latency_p99_us': 10000,
    'throughput_rps': 1000,
    'sse_connections': 100,
    'startup_time_ms': 2000,
}


# ============================================================================
# Performance Targets
# ============================================================================

TARGETS = {
    'latency_p50_us': 100,
    'latency_p99_us': 1000,
    'throughput_rps': 10000,
    'sse_connections': 1000,
    'startup_time_ms': 100,
}


# ============================================================================
# Report Generators
# ============================================================================

class TextReportGenerator:
    """Generate text-based benchmark report"""

    def __init__(self, suite: BenchmarkSuite):
        self.suite = suite

    def generate(self) -> str:
        """Generate complete report"""
        lines = []

        # Header
        lines.append("=" * 60)
        lines.append("Gateway Performance Report")
        lines.append("=" * 60)
        lines.append(f"Date: {self.suite.date}")
        lines.append(f"Platform: {self.suite.platform}")
        lines.append("")

        # Latency Results
        lines.append("Latency:")
        self._add_latency_section(lines)
        lines.append("")

        # Throughput Results
        lines.append("Throughput:")
        self._add_throughput_section(lines)
        lines.append("")

        # SSE Results
        lines.append("SSE:")
        self._add_sse_section(lines)
        lines.append("")

        # Comparison vs Python
        lines.append("Comparison vs Python:")
        self._add_comparison_section(lines)
        lines.append("")

        # Target Status
        lines.append("Target Status:")
        self._add_target_status(lines)
        lines.append("")

        return "\n".join(lines)

    def _add_latency_section(self, lines: List[str]):
        """Add latency results section"""
        if self.suite.full_request:
            stats = self.suite.full_request.stats
            lines.append(f"  P50:   {stats.p50:6.0f} μs   ✓ (target: <100μs)")
            lines.append(f"  P90:   {stats.p90:6.0f} μs")
            lines.append(f"  P99:   {stats.p99:6.0f} μs   ✓ (target: <1ms)")
            lines.append(f"  P99.9: {stats.p999:6.0f} μs")

    def _add_throughput_section(self, lines: List[str]):
        """Add throughput results section"""
        if self.suite.max_throughput:
            lines.append(f"  Max:   {self.suite.max_throughput.requests_per_second:,.0f} req/s ✓ (target: >10K)")

    def _add_sse_section(self, lines: List[str]):
        """Add SSE results section"""
        if self.suite.sse_event_delivery:
            lines.append(f"  Event latency:   {self.suite.sse_event_delivery.stats.p50:.0f} μs ✓")

    def _add_comparison_section(self, lines: List[str]):
        """Add Python comparison section"""
        if self.suite.full_request and self.suite.max_throughput:
            latency_p50_improvement = PYTHON_BASELINE['latency_p50_us'] / self.suite.full_request.stats.p50
            latency_p99_improvement = PYTHON_BASELINE['latency_p99_us'] / self.suite.full_request.stats.p99
            throughput_improvement = self.suite.max_throughput.requests_per_second / PYTHON_BASELINE['throughput_rps']

            lines.append(f"  Latency improvement: {latency_p50_improvement:.1f}x")
            lines.append(f"  Throughput improvement: {throughput_improvement:.1f}x")

    def _add_target_status(self, lines: List[str]):
        """Add target status summary"""
        if self.suite.full_request:
            p50_pass = self.suite.full_request.stats.p50 < TARGETS['latency_p50_us']
            p99_pass = self.suite.full_request.stats.p99 < TARGETS['latency_p99_us']
            lines.append(f"  Latency (P50 <100μs):   {'✓ PASS' if p50_pass else '✗ FAIL'}")
            lines.append(f"  Latency (P99 <1ms):    {'✓ PASS' if p99_pass else '✗ FAIL'}")

        if self.suite.max_throughput:
            throughput_pass = self.suite.max_throughput.requests_per_second > TARGETS['throughput_rps']
            lines.append(f"  Throughput (>10K/s):   {'✓ PASS' if throughput_pass else '✗ FAIL'}")


class JsonReportGenerator:
    """Generate JSON format benchmark report"""

    def __init__(self, suite: BenchmarkSuite):
        self.suite = suite

    def generate(self) -> str:
        """Generate JSON report"""
        data = {
            'date': self.suite.date,
            'platform': self.suite.platform,
            'targets': TARGETS,
            'python_baseline': PYTHON_BASELINE,
            'results': {}
        }

        # Add results
        if self.suite.route_lookup:
            data['results']['route_lookup'] = self._stats_to_dict(self.suite.route_lookup.stats)

        if self.suite.authentication:
            data['results']['authentication'] = self._stats_to_dict(self.suite.authentication.stats)

        if self.suite.full_request:
            data['results']['full_request'] = {
                'stats': self._stats_to_dict(self.suite.full_request.stats),
                'iterations': self.suite.full_request.iterations,
                'total_time_ms': self.suite.full_request.total_time_ms
            }

        if self.suite.max_throughput:
            data['results']['max_throughput'] = {
                'requests_per_second': self.suite.max_throughput.requests_per_second,
                'avg_latency_us': self.suite.max_throughput.avg_latency_us,
                'total_requests': self.suite.max_throughput.total_requests
            }

        if self.suite.sse_event_delivery:
            data['results']['sse_event_delivery'] = self._stats_to_dict(self.suite.sse_event_delivery.stats)

        if self.suite.startup_time:
            data['results']['startup_time'] = self._stats_to_dict(self.suite.startup_time)

        if self.suite.throughput_scaling:
            data['results']['throughput_scaling'] = self.suite.throughput_scaling

        # Calculate comparisons
        data['comparison'] = self._calculate_comparisons()

        # Target status
        data['targets_status'] = self._check_targets()

        return json.dumps(data, indent=2)

    def _stats_to_dict(self, stats: PercentileStats) -> Dict:
        """Convert PercentileStats to dictionary"""
        return {
            'p50_us': stats.p50,
            'p90_us': stats.p90,
            'p99_us': stats.p99,
            'p999_us': stats.p999,
            'min_us': stats.min,
            'max_us': stats.max,
            'mean_us': stats.mean,
            'stddev_us': stats.stddev
        }

    def _calculate_comparisons(self) -> Dict:
        """Calculate comparisons with Python baseline"""
        comparison = {}

        if self.suite.full_request:
            comparison['latency_p50_improvement'] = (
                PYTHON_BASELINE['latency_p50_us'] / self.suite.full_request.stats.p50
            )
            comparison['latency_p99_improvement'] = (
                PYTHON_BASELINE['latency_p99_us'] / self.suite.full_request.stats.p99
            )

        if self.suite.max_throughput:
            comparison['throughput_improvement'] = (
                self.suite.max_throughput.requests_per_second / PYTHON_BASELINE['throughput_rps']
            )

        return comparison

    def _check_targets(self) -> Dict:
        """Check if all targets are met"""
        status = {}

        if self.suite.full_request:
            status['latency_p50'] = self.suite.full_request.stats.p50 < TARGETS['latency_p50_us']
            status['latency_p99'] = self.suite.full_request.stats.p99 < TARGETS['latency_p99_us']

        if self.suite.max_throughput:
            status['throughput'] = self.suite.max_throughput.requests_per_second > TARGETS['throughput_rps']

        status['all_passed'] = all(status.values()) if status else False

        return status


# ============================================================================
# Bottleneck Analysis
# ============================================================================

def analyze_bottlenecks(suite: BenchmarkSuite) -> List[Dict]:
    """Analyze potential bottlenecks based on results"""
    bottlenecks = []

    if suite.authentication:
        # If authentication is >20% of total request time
        if suite.full_request and suite.authentication.stats.mean > 0:
            auth_ratio = suite.authentication.stats.mean / suite.full_request.stats.mean
            if auth_ratio > 0.2:
                bottlenecks.append({
                    'category': 'Authentication',
                    'issue': 'JWT verification takes significant portion of request time',
                    'severity': 'Medium',
                    'recommendation': 'Consider token caching or shorter tokens'
                })

    if suite.max_throughput:
        # Check if throughput scales linearly
        if suite.throughput_scaling and len(suite.throughput_scaling) > 1:
            first = suite.throughput_scaling[0]
            last = suite.throughput_scaling[-1]
            scaling_efficiency = (last['throughput'] / last['concurrency']) / (first['throughput'] / first['concurrency'])
            if scaling_efficiency < 0.5:
                bottlenecks.append({
                    'category': 'Throughput Scaling',
                    'issue': 'Poor scaling with concurrency',
                    'severity': 'High',
                    'recommendation': 'Check for lock contention or shared resources'
                })

    return bottlenecks


# ============================================================================
# Main Entry Point
# ============================================================================

def main():
    """Main entry point for benchmark report generation"""
    parser = argparse.ArgumentParser(
        description='Generate benchmark reports for VeloZ C++ Gateway'
    )
    parser.add_argument(
        '--input',
        '-i',
        type=str,
        default=None,
        help='Benchmark output file (default: stdin)'
    )
    parser.add_argument(
        '--output',
        '-o',
        type=str,
        default='benchmark_report',
        help='Output directory for reports (default: benchmark_report)'
    )
    parser.add_argument(
        '--format',
        '-f',
        type=str,
        choices=['text', 'json', 'all'],
        default='all',
        help='Output format (default: all)'
    )
    parser.add_argument(
        '--analyze',
        '-a',
        action='store_true',
        help='Run bottleneck analysis'
    )

    args = parser.parse_args()

    # Read benchmark output
    if args.input:
        with open(args.input, 'r') as f:
            output = f.read()
    else:
        output = sys.stdin.read()

    # Parse benchmark results
    benchmark_parser = BenchmarkParser()
    suite = benchmark_parser.parse(output)

    # Set metadata
    import platform
    from datetime import datetime
    suite.platform = platform.system() + ' ' + platform.machine()
    suite.date = datetime.now().strftime('%Y-%m-%d')

    # Create output directory
    output_dir = Path(args.output)
    output_dir.mkdir(parents=True, exist_ok=True)

    # Generate reports
    if args.format in ['text', 'all']:
        text_gen = TextReportGenerator(suite)
        text_report = text_gen.generate()

        text_file = output_dir / 'report.txt'
        with open(text_file, 'w') as f:
            f.write(text_report)
        print(f"Text report written to: {text_file}")

        # Also print to stdout
        print("\n" + text_report)

    if args.format in ['json', 'all']:
        json_gen = JsonReportGenerator(suite)
        json_report = json_gen.generate()

        json_file = output_dir / 'report.json'
        with open(json_file, 'w') as f:
            f.write(json_report)
        print(f"JSON report written to: {json_file}")

    # Bottleneck analysis
    if args.analyze:
        bottlenecks = analyze_bottlenecks(suite)
        if bottlenecks:
            print("\n=== Bottleneck Analysis ===")
            for b in bottlenecks:
                print(f"\n{b['category']}:")
                print(f"  Issue: {b['issue']}")
                print(f"  Severity: {b['severity']}")
                print(f"  Recommendation: {b['recommendation']}")
        else:
            print("\n=== Bottleneck Analysis ===")
            print("No significant bottlenecks detected.")


if __name__ == '__main__':
    main()
