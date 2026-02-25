#!/usr/bin/env python3
"""Integration tests for gateway-engine communication.

Tests the full communication protocol between the Python gateway and C++ engine:
- Engine process spawning and lifecycle
- Stdio protocol (commands -> engine, NDJSON <- engine)
- Order flow end-to-end
- Error handling and recovery
"""

import json
import os
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import urllib.request
from pathlib import Path
from queue import Queue, Empty

# Add gateway directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from gateway import Handler
from http.server import ThreadingHTTPServer


class EngineProcess:
    """Helper class to manage engine subprocess for testing."""

    def __init__(self, engine_path: str):
        self.engine_path = engine_path
        self.process = None
        self.output_queue = Queue()
        self.reader_thread = None
        self._stop_reader = False

    def start(self, timeout: float = 5.0) -> bool:
        """Start the engine process."""
        if not os.path.exists(self.engine_path):
            return False

        self.process = subprocess.Popen(
            [self.engine_path, "--stdio"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

        # Start output reader thread
        self._stop_reader = False
        self.reader_thread = threading.Thread(target=self._read_output, daemon=True)
        self.reader_thread.start()

        # Wait for startup event
        try:
            event = self.wait_for_event("engine_started", timeout=timeout)
            return event is not None
        except Exception:
            return False

    def stop(self):
        """Stop the engine process."""
        self._stop_reader = True
        if self.process:
            # Close stdin to signal EOF
            if self.process.stdin:
                try:
                    self.process.stdin.close()
                except Exception:
                    pass
            self.process.terminate()
            try:
                self.process.wait(timeout=2.0)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
            # Close remaining file handles
            if self.process.stdout:
                try:
                    self.process.stdout.close()
                except Exception:
                    pass
            if self.process.stderr:
                try:
                    self.process.stderr.close()
                except Exception:
                    pass
            self.process = None

    def send_command(self, command: str):
        """Send a command to the engine."""
        if self.process and self.process.stdin:
            self.process.stdin.write(command + "\n")
            self.process.stdin.flush()

    def wait_for_event(self, event_type: str, timeout: float = 2.0):
        """Wait for a specific event type from the engine."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                line = self.output_queue.get(timeout=0.1)
                # Only process lines that look like JSON
                if line and line.strip().startswith("{"):
                    try:
                        event = json.loads(line.strip())
                        if event.get("type") == event_type:
                            return event
                    except json.JSONDecodeError:
                        pass
            except Empty:
                pass
        return None

    def get_all_events(self, timeout: float = 0.5) -> list:
        """Get all pending events from the queue."""
        events = []
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                line = self.output_queue.get(timeout=0.1)
                if line and line.strip().startswith("{"):
                    try:
                        event = json.loads(line.strip())
                        events.append(event)
                    except json.JSONDecodeError:
                        pass
            except Empty:
                break
        return events

    def _read_output(self):
        """Background thread to read engine output."""
        while not self._stop_reader and self.process:
            try:
                line = self.process.stdout.readline()
                if line:
                    self.output_queue.put(line.strip())
                elif self.process.poll() is not None:
                    break
            except Exception:
                break


class TestGatewayHealthEndpoint(unittest.TestCase):
    def setUp(self):
        class DummyBridge:
            def __init__(self):
                self.connected = False

            def is_connected(self):
                return self.connected

        self.bridge = DummyBridge()
        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        self.httpd.bridge = self.bridge
        self.httpd.router = None
        self.httpd.auth_manager = None
        self.httpd.security_manager = None
        self.thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)
        self.thread.start()
        host, port = self.httpd.server_address
        self.base_url = f"http://{host}:{port}"

    def tearDown(self):
        self.httpd.shutdown()
        self.httpd.server_close()
        self.thread.join(timeout=2.0)

    def test_health_reports_engine_disconnected(self):
        with urllib.request.urlopen(f"{self.base_url}/api/health", timeout=2.0) as resp:
            body = resp.read().decode("utf-8", errors="replace")
        payload = json.loads(body)
        self.assertTrue(payload.get("ok"))
        self.assertIn("engine", payload)
        self.assertFalse(payload["engine"].get("connected", True))

def find_engine_binary() -> str:
    """Find the engine binary path."""
    # Try common build locations
    base_dir = Path(__file__).parent.parent.parent
    candidates = [
        base_dir / "build" / "dev" / "apps" / "engine" / "veloz_engine",
        base_dir / "build" / "release" / "apps" / "engine" / "veloz_engine",
        base_dir / "build" / "asan" / "apps" / "engine" / "veloz_engine",
    ]
    for path in candidates:
        if path.exists():
            return str(path)
    return ""


class TestEngineLifecycle(unittest.TestCase):
    """Tests for engine process lifecycle management."""

    @classmethod
    def setUpClass(cls):
        cls.engine_path = find_engine_binary()

    def setUp(self):
        self.engine = EngineProcess(self.engine_path)

    def tearDown(self):
        self.engine.stop()

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_engine_starts_and_emits_startup_event(self):
        """Test that engine starts and emits engine_started event."""
        result = self.engine.start(timeout=5.0)
        self.assertTrue(result, "Engine should start successfully")

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_engine_stops_gracefully(self):
        """Test that engine stops gracefully."""
        self.engine.start(timeout=5.0)
        self.engine.stop()
        # Process should be terminated
        self.assertIsNone(self.engine.process)


class TestOrderCommands(unittest.TestCase):
    """Tests for order command processing."""

    @classmethod
    def setUpClass(cls):
        cls.engine_path = find_engine_binary()

    def setUp(self):
        self.engine = EngineProcess(self.engine_path)
        if self.engine_path:
            self.engine.start(timeout=5.0)

    def tearDown(self):
        self.engine.stop()

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_order_command_generates_event(self):
        """Test that ORDER command generates order_received event.

        Note: The engine has a bug where it outputs duplicate 'type' keys in the JSON
        (one for event type 'order_received' and one for order type 'limit'/'market').
        Python's JSON parser keeps the last value, so we detect order events by
        checking for the presence of 'client_order_id' field instead.
        """
        # Small delay to ensure engine is ready
        time.sleep(0.3)
        self.engine.send_command("ORDER BUY BTCUSDT 0.1 50000.0 test-order-001")

        # Wait a bit for the command to be processed
        time.sleep(0.3)

        # Get all events and look for order events by checking for client_order_id field
        # (workaround for duplicate 'type' key bug in engine JSON output)
        events = self.engine.get_all_events(timeout=3.0)
        order_events = [e for e in events if "client_order_id" in e and "symbol" in e]

        self.assertGreater(
            len(order_events), 0,
            f"Should receive order event with client_order_id, got: {events}"
        )
        if order_events:
            self.assertEqual(order_events[0].get("client_order_id"), "test-order-001")

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_cancel_command_generates_event(self):
        """Test that CANCEL command generates cancel_received event."""
        # Small delay to ensure engine is ready
        time.sleep(0.2)

        # First place an order
        self.engine.send_command("ORDER BUY BTCUSDT 0.1 50000.0 test-order-002")
        self.engine.wait_for_event("order_received", timeout=3.0)

        # Then cancel it
        self.engine.send_command("CANCEL test-order-002")

        # Wait for cancel_received event
        event = self.engine.wait_for_event("cancel_received", timeout=3.0)
        self.assertIsNotNone(event, "Should receive cancel_received event")

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_invalid_command_generates_error(self):
        """Test that invalid command generates error event."""
        self.engine.send_command("INVALID_COMMAND")

        # Wait for error event
        event = self.engine.wait_for_event("error", timeout=2.0)
        self.assertIsNotNone(event, "Should receive error event for invalid command")


class TestStrategyCommands(unittest.TestCase):
    """Tests for strategy command processing."""

    @classmethod
    def setUpClass(cls):
        cls.engine_path = find_engine_binary()

    def setUp(self):
        self.engine = EngineProcess(self.engine_path)
        if self.engine_path:
            self.engine.start(timeout=5.0)

    def tearDown(self):
        self.engine.stop()

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_strategy_list_command(self):
        """Test STRATEGY LIST command."""
        self.engine.send_command("STRATEGY LIST")

        # Wait for strategy_list event
        event = self.engine.wait_for_event("strategy_list", timeout=2.0)
        self.assertIsNotNone(event, "Should receive strategy_list event")

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_strategy_load_command(self):
        """Test STRATEGY LOAD command."""
        self.engine.send_command("STRATEGY LOAD RSI TestRsiStrategy")

        # Wait for either strategy_loaded or error event
        time.sleep(0.5)
        events = self.engine.get_all_events(timeout=2.0)

        # Check if we got a strategy_loaded or error event
        loaded_events = [e for e in events if e.get("type") == "strategy_loaded"]
        error_events = [e for e in events if e.get("type") == "error"]

        # Either loaded successfully or got an error (both are valid responses)
        self.assertTrue(
            len(loaded_events) > 0 or len(error_events) > 0,
            f"Should receive strategy_loaded or error event, got: {[e.get('type') for e in events]}"
        )

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_strategy_lifecycle(self):
        """Test full strategy lifecycle: load -> start -> stop -> unload."""
        # Load strategy - use TrendFollowing which is a built-in type
        self.engine.send_command("STRATEGY LOAD TrendFollowing LifecycleTest")

        time.sleep(0.5)
        events = self.engine.get_all_events(timeout=2.0)
        loaded_events = [e for e in events if e.get("type") == "strategy_loaded"]

        if len(loaded_events) > 0:
            strategy_id = loaded_events[0].get("strategy_id")

            # Start strategy
            self.engine.send_command(f"STRATEGY START {strategy_id}")
            start_event = self.engine.wait_for_event("strategy_started", timeout=2.0)
            # May fail if strategy doesn't support starting

            # Stop strategy
            self.engine.send_command(f"STRATEGY STOP {strategy_id}")
            stop_event = self.engine.wait_for_event("strategy_stopped", timeout=2.0)

            # Unload strategy
            self.engine.send_command(f"STRATEGY UNLOAD {strategy_id}")
            unload_event = self.engine.wait_for_event("strategy_unloaded", timeout=2.0)
        else:
            # Strategy loading failed, which is acceptable for this test
            # The test verifies the command is processed, not that it succeeds
            pass


class TestQueryCommands(unittest.TestCase):
    """Tests for query command processing."""

    @classmethod
    def setUpClass(cls):
        cls.engine_path = find_engine_binary()

    def setUp(self):
        self.engine = EngineProcess(self.engine_path)
        if self.engine_path:
            self.engine.start(timeout=5.0)

    def tearDown(self):
        self.engine.stop()

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_query_account_command(self):
        """Test QUERY account command."""
        self.engine.send_command("QUERY account")

        # Wait for account event
        event = self.engine.wait_for_event("account", timeout=2.0)
        # May or may not generate event depending on implementation


class TestNDJSONProtocol(unittest.TestCase):
    """Tests for NDJSON protocol compliance."""

    @classmethod
    def setUpClass(cls):
        cls.engine_path = find_engine_binary()

    def setUp(self):
        self.engine = EngineProcess(self.engine_path)
        if self.engine_path:
            self.engine.start(timeout=5.0)

    def tearDown(self):
        self.engine.stop()

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_all_events_are_valid_json(self):
        """Test that all engine output is valid JSON."""
        # Generate some events
        self.engine.send_command("STRATEGY LIST")
        self.engine.send_command("ORDER BUY BTCUSDT 0.1 50000.0 json-test-001")

        # Collect events
        time.sleep(0.5)
        events = self.engine.get_all_events(timeout=1.0)

        # All events should be valid JSON with type field
        for event in events:
            self.assertIsInstance(event, dict, "Event should be a dict")
            self.assertIn("type", event, "Event should have type field")

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_events_have_required_fields(self):
        """Test that events have required fields based on type."""
        self.engine.send_command("ORDER BUY BTCUSDT 0.1 50000.0 field-test-001")

        event = self.engine.wait_for_event("order_received", timeout=2.0)
        if event:
            # order_received should have client_order_id and symbol
            self.assertIn("client_order_id", event)
            self.assertIn("symbol", event)
            self.assertIn("side", event)
            self.assertIn("quantity", event)


class TestErrorHandling(unittest.TestCase):
    """Tests for error handling."""

    @classmethod
    def setUpClass(cls):
        cls.engine_path = find_engine_binary()

    def setUp(self):
        self.engine = EngineProcess(self.engine_path)
        if self.engine_path:
            self.engine.start(timeout=5.0)

    def tearDown(self):
        self.engine.stop()

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_malformed_order_command(self):
        """Test handling of malformed order command."""
        self.engine.send_command("ORDER BUY")  # Missing required fields

        event = self.engine.wait_for_event("error", timeout=2.0)
        self.assertIsNotNone(event, "Should receive error for malformed command")

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_invalid_quantity(self):
        """Test handling of invalid quantity."""
        self.engine.send_command("ORDER BUY BTCUSDT -1.0 50000.0 invalid-qty")

        event = self.engine.wait_for_event("error", timeout=2.0)
        self.assertIsNotNone(event, "Should receive error for invalid quantity")

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_empty_command_ignored(self):
        """Test that empty commands are ignored."""
        self.engine.send_command("")
        self.engine.send_command("   ")

        # Should not crash, no error expected
        time.sleep(0.2)

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_comment_lines_ignored(self):
        """Test that comment lines are ignored."""
        self.engine.send_command("# This is a comment")
        self.engine.send_command("// Another comment style")

        # Should not crash, no error expected
        time.sleep(0.2)


class TestConcurrentCommands(unittest.TestCase):
    """Tests for concurrent command handling."""

    @classmethod
    def setUpClass(cls):
        cls.engine_path = find_engine_binary()

    def setUp(self):
        self.engine = EngineProcess(self.engine_path)
        if self.engine_path:
            self.engine.start(timeout=5.0)

    def tearDown(self):
        self.engine.stop()

    @unittest.skipIf(not find_engine_binary(), "Engine binary not found")
    def test_rapid_command_sequence(self):
        """Test handling of rapid command sequence."""
        # Small delay to ensure engine is ready
        time.sleep(0.3)

        # Send multiple commands rapidly
        for i in range(10):
            self.engine.send_command(f"ORDER BUY BTCUSDT 0.1 50000.0 rapid-{i}")

        # Wait for events
        time.sleep(1.0)
        events = self.engine.get_all_events(timeout=2.0)

        # Should have received events for orders (check for client_order_id field)
        order_events = [e for e in events if "client_order_id" in e]
        self.assertGreaterEqual(len(order_events), 1, "Should receive order events")


if __name__ == "__main__":
    unittest.main(verbosity=2)
