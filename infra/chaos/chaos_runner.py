#!/usr/bin/env python3
"""
VeloZ Chaos Engineering Runner

Runs chaos experiments against VeloZ services in Docker or local environments.
For Kubernetes, use Chaos Mesh directly with the YAML experiments.

Usage:
    python chaos_runner.py --scenario network-latency
    python chaos_runner.py --all
    python chaos_runner.py --list
"""

import argparse
import json
import logging
import os
import random
import signal
import subprocess
import sys
import threading
import time
import urllib.error
import urllib.request
from dataclasses import dataclass, field
from typing import Callable, Optional

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
logger = logging.getLogger("chaos")


@dataclass
class ChaosExperiment:
    """Definition of a chaos experiment."""

    name: str
    description: str
    duration_seconds: int
    setup: Callable[[], bool]
    teardown: Callable[[], None]
    verify: Callable[[], bool]
    tags: list[str] = field(default_factory=list)


@dataclass
class ChaosResult:
    """Result of a chaos experiment."""

    name: str
    success: bool
    duration_seconds: float
    error: Optional[str] = None
    metrics: dict = field(default_factory=dict)


class ChaosRunner:
    """Runs chaos experiments against VeloZ services."""

    def __init__(
        self,
        gateway_url: str = "http://127.0.0.1:8080",
        docker_compose_file: str = "docker-compose.yml",
    ):
        self.gateway_url = gateway_url
        self.docker_compose_file = docker_compose_file
        self.experiments: dict[str, ChaosExperiment] = {}
        self._stop_event = threading.Event()
        self._register_experiments()

    def _register_experiments(self) -> None:
        """Register all chaos experiments."""

        # Network latency experiment
        self.experiments["network-latency"] = ChaosExperiment(
            name="network-latency",
            description="Add network latency to gateway container",
            duration_seconds=60,
            setup=lambda: self._add_network_latency("veloz-gateway", "200ms"),
            teardown=lambda: self._remove_network_latency("veloz-gateway"),
            verify=lambda: self._verify_service_degraded(),
            tags=["network", "latency"],
        )

        # Network partition experiment
        self.experiments["network-partition"] = ChaosExperiment(
            name="network-partition",
            description="Isolate gateway from external network",
            duration_seconds=30,
            setup=lambda: self._add_network_partition("veloz-gateway"),
            teardown=lambda: self._remove_network_partition("veloz-gateway"),
            verify=lambda: self._verify_service_unavailable(),
            tags=["network", "partition"],
        )

        # Container kill experiment
        self.experiments["container-kill"] = ChaosExperiment(
            name="container-kill",
            description="Kill and restart gateway container",
            duration_seconds=60,
            setup=lambda: self._kill_container("veloz-gateway"),
            teardown=lambda: self._restart_container("veloz-gateway"),
            verify=lambda: self._verify_service_recovered(),
            tags=["container", "kill"],
        )

        # CPU stress experiment
        self.experiments["cpu-stress"] = ChaosExperiment(
            name="cpu-stress",
            description="Stress CPU in gateway container",
            duration_seconds=120,
            setup=lambda: self._stress_cpu("veloz-gateway", workers=2),
            teardown=lambda: self._stop_stress("veloz-gateway"),
            verify=lambda: self._verify_service_slow(),
            tags=["stress", "cpu"],
        )

        # Memory stress experiment
        self.experiments["memory-stress"] = ChaosExperiment(
            name="memory-stress",
            description="Stress memory in gateway container",
            duration_seconds=120,
            setup=lambda: self._stress_memory("veloz-gateway", size="256M"),
            teardown=lambda: self._stop_stress("veloz-gateway"),
            verify=lambda: self._verify_service_slow(),
            tags=["stress", "memory"],
        )

        # Exchange failure simulation
        self.experiments["exchange-failure"] = ChaosExperiment(
            name="exchange-failure",
            description="Simulate Binance API failure",
            duration_seconds=60,
            setup=lambda: self._block_external_host("api.binance.com"),
            teardown=lambda: self._unblock_external_host("api.binance.com"),
            verify=lambda: self._verify_graceful_degradation(),
            tags=["external", "exchange"],
        )

        # Disk fill experiment
        self.experiments["disk-fill"] = ChaosExperiment(
            name="disk-fill",
            description="Fill disk space in container",
            duration_seconds=60,
            setup=lambda: self._fill_disk("veloz-gateway", size="100M"),
            teardown=lambda: self._clear_disk("veloz-gateway"),
            verify=lambda: self._verify_service_healthy(),
            tags=["disk", "storage"],
        )

    def run_experiment(self, name: str) -> ChaosResult:
        """Run a single chaos experiment."""
        if name not in self.experiments:
            return ChaosResult(
                name=name,
                success=False,
                duration_seconds=0,
                error=f"Unknown experiment: {name}",
            )

        experiment = self.experiments[name]
        logger.info("=" * 60)
        logger.info(f"Starting experiment: {experiment.name}")
        logger.info(f"Description: {experiment.description}")
        logger.info(f"Duration: {experiment.duration_seconds}s")
        logger.info("=" * 60)

        start_time = time.time()
        error = None
        success = False

        try:
            # Setup chaos
            logger.info("Setting up chaos condition...")
            if not experiment.setup():
                raise RuntimeError("Failed to setup chaos condition")

            # Wait for duration
            logger.info(f"Chaos active for {experiment.duration_seconds}s...")
            self._wait_with_monitoring(experiment.duration_seconds)

            # Verify behavior
            logger.info("Verifying system behavior...")
            success = experiment.verify()
            if not success:
                logger.warning("Verification failed - system did not behave as expected")

        except Exception as e:
            error = str(e)
            logger.error(f"Experiment failed: {error}")

        finally:
            # Teardown
            logger.info("Tearing down chaos condition...")
            try:
                experiment.teardown()
            except Exception as e:
                logger.error(f"Teardown failed: {e}")

        duration = time.time() - start_time

        result = ChaosResult(
            name=name,
            success=success and error is None,
            duration_seconds=duration,
            error=error,
        )

        logger.info(f"Experiment completed: {'PASS' if result.success else 'FAIL'}")
        return result

    def run_all(self, tags: Optional[list[str]] = None) -> list[ChaosResult]:
        """Run all experiments, optionally filtered by tags."""
        results = []

        experiments_to_run = self.experiments.values()
        if tags:
            experiments_to_run = [
                e for e in experiments_to_run if any(t in e.tags for t in tags)
            ]

        for experiment in experiments_to_run:
            if self._stop_event.is_set():
                logger.info("Stopping experiments due to interrupt")
                break

            result = self.run_experiment(experiment.name)
            results.append(result)

            # Brief pause between experiments
            time.sleep(10)

        return results

    def list_experiments(self) -> None:
        """List all available experiments."""
        print("\nAvailable Chaos Experiments:")
        print("-" * 60)
        for name, exp in sorted(self.experiments.items()):
            tags = ", ".join(exp.tags) if exp.tags else "none"
            print(f"  {name}")
            print(f"    Description: {exp.description}")
            print(f"    Duration: {exp.duration_seconds}s")
            print(f"    Tags: {tags}")
            print()

    # =========================================================================
    # Chaos Implementation Methods
    # =========================================================================

    def _add_network_latency(self, container: str, latency: str) -> bool:
        """Add network latency using tc."""
        try:
            cmd = f"docker exec {container} tc qdisc add dev eth0 root netem delay {latency}"
            subprocess.run(cmd, shell=True, check=True, capture_output=True)
            logger.info(f"Added {latency} latency to {container}")
            return True
        except subprocess.CalledProcessError as e:
            logger.error(f"Failed to add latency: {e}")
            return False

    def _remove_network_latency(self, container: str) -> None:
        """Remove network latency."""
        try:
            cmd = f"docker exec {container} tc qdisc del dev eth0 root 2>/dev/null || true"
            subprocess.run(cmd, shell=True, capture_output=True)
            logger.info(f"Removed latency from {container}")
        except Exception as e:
            logger.warning(f"Failed to remove latency: {e}")

    def _add_network_partition(self, container: str) -> bool:
        """Partition container from network."""
        try:
            cmd = f"docker network disconnect veloz-network {container}"
            subprocess.run(cmd, shell=True, check=True, capture_output=True)
            logger.info(f"Disconnected {container} from network")
            return True
        except subprocess.CalledProcessError as e:
            logger.error(f"Failed to partition: {e}")
            return False

    def _remove_network_partition(self, container: str) -> None:
        """Restore network connection."""
        try:
            cmd = f"docker network connect veloz-network {container}"
            subprocess.run(cmd, shell=True, capture_output=True)
            logger.info(f"Reconnected {container} to network")
        except Exception as e:
            logger.warning(f"Failed to restore network: {e}")

    def _kill_container(self, container: str) -> bool:
        """Kill a container."""
        try:
            cmd = f"docker kill {container}"
            subprocess.run(cmd, shell=True, check=True, capture_output=True)
            logger.info(f"Killed container {container}")
            return True
        except subprocess.CalledProcessError as e:
            logger.error(f"Failed to kill container: {e}")
            return False

    def _restart_container(self, container: str) -> None:
        """Restart a container."""
        try:
            cmd = f"docker start {container}"
            subprocess.run(cmd, shell=True, capture_output=True)
            logger.info(f"Restarted container {container}")
        except Exception as e:
            logger.warning(f"Failed to restart container: {e}")

    def _stress_cpu(self, container: str, workers: int = 2) -> bool:
        """Stress CPU in container."""
        try:
            cmd = f"docker exec -d {container} stress-ng --cpu {workers} --timeout 300s"
            subprocess.run(cmd, shell=True, check=True, capture_output=True)
            logger.info(f"Started CPU stress in {container}")
            return True
        except subprocess.CalledProcessError:
            # stress-ng might not be installed, try with dd
            try:
                for i in range(workers):
                    cmd = f"docker exec -d {container} sh -c 'while true; do :; done &'"
                    subprocess.run(cmd, shell=True, capture_output=True)
                logger.info(f"Started CPU stress in {container} (fallback)")
                return True
            except Exception as e:
                logger.error(f"Failed to stress CPU: {e}")
                return False

    def _stress_memory(self, container: str, size: str = "256M") -> bool:
        """Stress memory in container."""
        try:
            cmd = f"docker exec -d {container} stress-ng --vm 1 --vm-bytes {size} --timeout 300s"
            subprocess.run(cmd, shell=True, check=True, capture_output=True)
            logger.info(f"Started memory stress in {container}")
            return True
        except subprocess.CalledProcessError as e:
            logger.error(f"Failed to stress memory: {e}")
            return False

    def _stop_stress(self, container: str) -> None:
        """Stop stress processes."""
        try:
            cmd = f"docker exec {container} pkill -9 stress-ng 2>/dev/null || true"
            subprocess.run(cmd, shell=True, capture_output=True)
            cmd = f"docker exec {container} pkill -9 'while true' 2>/dev/null || true"
            subprocess.run(cmd, shell=True, capture_output=True)
            logger.info(f"Stopped stress in {container}")
        except Exception as e:
            logger.warning(f"Failed to stop stress: {e}")

    def _block_external_host(self, host: str) -> bool:
        """Block external host using iptables."""
        try:
            cmd = f"docker exec veloz-gateway iptables -A OUTPUT -d {host} -j DROP"
            subprocess.run(cmd, shell=True, check=True, capture_output=True)
            logger.info(f"Blocked external host {host}")
            return True
        except subprocess.CalledProcessError as e:
            logger.error(f"Failed to block host: {e}")
            return False

    def _unblock_external_host(self, host: str) -> None:
        """Unblock external host."""
        try:
            cmd = f"docker exec veloz-gateway iptables -D OUTPUT -d {host} -j DROP 2>/dev/null || true"
            subprocess.run(cmd, shell=True, capture_output=True)
            logger.info(f"Unblocked external host {host}")
        except Exception as e:
            logger.warning(f"Failed to unblock host: {e}")

    def _fill_disk(self, container: str, size: str = "100M") -> bool:
        """Fill disk space in container."""
        try:
            cmd = f"docker exec {container} dd if=/dev/zero of=/tmp/chaos_fill bs=1M count={size.rstrip('M')} 2>/dev/null"
            subprocess.run(cmd, shell=True, capture_output=True)
            logger.info(f"Filled {size} disk space in {container}")
            return True
        except Exception as e:
            logger.error(f"Failed to fill disk: {e}")
            return False

    def _clear_disk(self, container: str) -> None:
        """Clear disk fill."""
        try:
            cmd = f"docker exec {container} rm -f /tmp/chaos_fill"
            subprocess.run(cmd, shell=True, capture_output=True)
            logger.info(f"Cleared disk fill in {container}")
        except Exception as e:
            logger.warning(f"Failed to clear disk: {e}")

    # =========================================================================
    # Verification Methods
    # =========================================================================

    def _check_health(self) -> bool:
        """Check if service is healthy."""
        try:
            req = urllib.request.Request(f"{self.gateway_url}/health")
            with urllib.request.urlopen(req, timeout=5) as resp:
                return resp.status == 200
        except Exception:
            return False

    def _verify_service_healthy(self) -> bool:
        """Verify service is healthy."""
        return self._check_health()

    def _verify_service_degraded(self) -> bool:
        """Verify service is degraded but responding."""
        start = time.time()
        try:
            req = urllib.request.Request(f"{self.gateway_url}/health")
            with urllib.request.urlopen(req, timeout=10) as resp:
                latency = time.time() - start
                # Should be slow but working
                return resp.status == 200 and latency > 0.1
        except Exception:
            return False

    def _verify_service_unavailable(self) -> bool:
        """Verify service is unavailable."""
        return not self._check_health()

    def _verify_service_recovered(self) -> bool:
        """Verify service recovered after chaos."""
        for _ in range(10):
            if self._check_health():
                return True
            time.sleep(5)
        return False

    def _verify_service_slow(self) -> bool:
        """Verify service is slow but responding."""
        return self._verify_service_degraded()

    def _verify_graceful_degradation(self) -> bool:
        """Verify service degrades gracefully."""
        # Service should still respond to health checks
        return self._check_health()

    def _wait_with_monitoring(self, duration: int) -> None:
        """Wait while monitoring service health."""
        end_time = time.time() + duration
        while time.time() < end_time and not self._stop_event.is_set():
            health = "healthy" if self._check_health() else "unhealthy"
            logger.debug(f"Service status: {health}")
            time.sleep(5)

    def stop(self) -> None:
        """Signal experiments to stop."""
        self._stop_event.set()


def main():
    parser = argparse.ArgumentParser(description="VeloZ Chaos Engineering Runner")
    parser.add_argument("--scenario", "-s", help="Run specific scenario")
    parser.add_argument("--all", "-a", action="store_true", help="Run all scenarios")
    parser.add_argument("--list", "-l", action="store_true", help="List scenarios")
    parser.add_argument("--tags", "-t", nargs="+", help="Filter by tags")
    parser.add_argument(
        "--gateway-url",
        default="http://127.0.0.1:8080",
        help="Gateway URL",
    )
    parser.add_argument("--output", "-o", help="Output results to JSON file")

    args = parser.parse_args()

    runner = ChaosRunner(gateway_url=args.gateway_url)

    # Handle Ctrl+C gracefully
    def signal_handler(sig, frame):
        logger.info("Received interrupt, stopping...")
        runner.stop()

    signal.signal(signal.SIGINT, signal_handler)

    if args.list:
        runner.list_experiments()
        return

    results = []

    if args.scenario:
        result = runner.run_experiment(args.scenario)
        results.append(result)
    elif args.all:
        results = runner.run_all(tags=args.tags)
    else:
        parser.print_help()
        return

    # Print summary
    print("\n" + "=" * 60)
    print("CHAOS EXPERIMENT RESULTS")
    print("=" * 60)

    passed = sum(1 for r in results if r.success)
    failed = len(results) - passed

    for result in results:
        status = "PASS" if result.success else "FAIL"
        print(f"  [{status}] {result.name} ({result.duration_seconds:.1f}s)")
        if result.error:
            print(f"         Error: {result.error}")

    print("-" * 60)
    print(f"Total: {len(results)} | Passed: {passed} | Failed: {failed}")

    # Output to file if requested
    if args.output:
        output_data = {
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "summary": {"total": len(results), "passed": passed, "failed": failed},
            "results": [
                {
                    "name": r.name,
                    "success": r.success,
                    "duration_seconds": r.duration_seconds,
                    "error": r.error,
                }
                for r in results
            ],
        }
        with open(args.output, "w") as f:
            json.dump(output_data, f, indent=2)
        print(f"\nResults written to {args.output}")

    # Exit with error code if any failed
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
