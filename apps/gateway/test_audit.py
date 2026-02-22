#!/usr/bin/env python3
"""Unit tests for audit log retention policies."""

import unittest
import tempfile
import shutil
import time
import os
import sys
from pathlib import Path
from datetime import datetime, timedelta

# Add gateway directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from audit import (
    RetentionPolicy,
    AuditLogEntry,
    AuditLogStore,
    AuditLogRetentionManager,
    DEFAULT_POLICIES,
    create_audit_entry,
)


class TestRetentionPolicy(unittest.TestCase):
    """Tests for RetentionPolicy dataclass."""

    def test_default_values(self):
        """Test default retention policy values."""
        policy = RetentionPolicy(log_type="test")
        self.assertEqual(policy.retention_days, 30)
        self.assertTrue(policy.archive_before_delete)
        self.assertEqual(policy.max_file_size_mb, 100)
        self.assertEqual(policy.max_files, 10)

    def test_custom_values(self):
        """Test custom retention policy values."""
        policy = RetentionPolicy(
            log_type="custom",
            retention_days=90,
            archive_before_delete=False,
            max_file_size_mb=50,
            max_files=5,
        )
        self.assertEqual(policy.log_type, "custom")
        self.assertEqual(policy.retention_days, 90)
        self.assertFalse(policy.archive_before_delete)


class TestAuditLogEntry(unittest.TestCase):
    """Tests for AuditLogEntry dataclass."""

    def test_create_entry(self):
        """Test creating an audit log entry."""
        entry = AuditLogEntry(
            timestamp=time.time(),
            log_type="auth",
            action="login",
            user_id="user123",
            ip_address="192.168.1.1",
            details={"method": "password"},
        )
        self.assertEqual(entry.log_type, "auth")
        self.assertEqual(entry.action, "login")
        self.assertEqual(entry.user_id, "user123")

    def test_to_dict(self):
        """Test converting entry to dictionary."""
        ts = time.time()
        entry = AuditLogEntry(
            timestamp=ts,
            log_type="auth",
            action="login",
            user_id="user123",
            ip_address="192.168.1.1",
        )
        d = entry.to_dict()
        self.assertEqual(d["timestamp"], ts)
        self.assertEqual(d["log_type"], "auth")
        self.assertIn("datetime", d)

    def test_to_json(self):
        """Test converting entry to JSON string."""
        entry = AuditLogEntry(
            timestamp=time.time(),
            log_type="auth",
            action="login",
            user_id="user123",
            ip_address="192.168.1.1",
        )
        json_str = entry.to_json()
        self.assertIn('"log_type": "auth"', json_str)
        self.assertIn('"action": "login"', json_str)


class TestCreateAuditEntry(unittest.TestCase):
    """Tests for create_audit_entry helper function."""

    def test_create_entry_with_defaults(self):
        """Test creating entry with default values."""
        entry = create_audit_entry(
            log_type="auth",
            action="login",
            user_id="user123",
            ip_address="192.168.1.1",
        )
        self.assertIsInstance(entry, AuditLogEntry)
        self.assertGreater(entry.timestamp, 0)
        self.assertEqual(entry.details, {})

    def test_create_entry_with_details(self):
        """Test creating entry with custom details."""
        entry = create_audit_entry(
            log_type="order",
            action="place_order",
            user_id="user123",
            ip_address="192.168.1.1",
            details={"symbol": "BTCUSDT", "qty": 0.1},
            request_id="req-123",
        )
        self.assertEqual(entry.details["symbol"], "BTCUSDT")
        self.assertEqual(entry.request_id, "req-123")


class TestDefaultPolicies(unittest.TestCase):
    """Tests for default retention policies."""

    def test_auth_policy(self):
        """Test auth log retention policy."""
        policy = DEFAULT_POLICIES["auth"]
        self.assertEqual(policy.retention_days, 90)
        self.assertTrue(policy.archive_before_delete)

    def test_order_policy(self):
        """Test order log retention policy."""
        policy = DEFAULT_POLICIES["order"]
        self.assertEqual(policy.retention_days, 365)
        self.assertTrue(policy.archive_before_delete)

    def test_access_policy(self):
        """Test access log retention policy."""
        policy = DEFAULT_POLICIES["access"]
        self.assertEqual(policy.retention_days, 14)
        self.assertFalse(policy.archive_before_delete)

    def test_default_policy(self):
        """Test default fallback policy."""
        policy = DEFAULT_POLICIES["default"]
        self.assertEqual(policy.retention_days, 30)


class TestAuditLogStore(unittest.TestCase):
    """Tests for AuditLogStore class."""

    def setUp(self):
        """Create temporary directory for tests."""
        self.temp_dir = tempfile.mkdtemp()
        self.store = AuditLogStore(log_dir=self.temp_dir)

    def tearDown(self):
        """Clean up temporary directory."""
        self.store.close()
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_get_default_policy(self):
        """Test getting default policy for unknown log type."""
        policy = self.store.get_policy("unknown_type")
        self.assertEqual(policy.log_type, "default")

    def test_get_specific_policy(self):
        """Test getting policy for specific log type."""
        policy = self.store.get_policy("auth")
        self.assertEqual(policy.log_type, "auth")
        self.assertEqual(policy.retention_days, 90)

    def test_set_custom_policy(self):
        """Test setting a custom policy."""
        custom_policy = RetentionPolicy(
            log_type="custom",
            retention_days=7,
        )
        self.store.set_policy(custom_policy)
        policy = self.store.get_policy("custom")
        self.assertEqual(policy.retention_days, 7)

    def test_write_entry(self):
        """Test writing an audit log entry."""
        entry = create_audit_entry(
            log_type="auth",
            action="login",
            user_id="user123",
            ip_address="192.168.1.1",
        )
        self.store.write(entry)

        # Check that file was created
        date_str = datetime.now().strftime("%Y-%m-%d")
        expected_file = Path(self.temp_dir) / f"auth_{date_str}.ndjson"
        self.assertTrue(expected_file.exists())

    def test_get_recent_logs(self):
        """Test getting recent logs from memory."""
        for i in range(5):
            entry = create_audit_entry(
                log_type="auth",
                action=f"action_{i}",
                user_id="user123",
                ip_address="192.168.1.1",
            )
            self.store.write(entry)

        recent = self.store.get_recent_logs(limit=3)
        self.assertEqual(len(recent), 3)

    def test_get_recent_logs_filtered_by_type(self):
        """Test filtering recent logs by type."""
        self.store.write(create_audit_entry("auth", "login", "user1", "1.1.1.1"))
        self.store.write(create_audit_entry("order", "place", "user1", "1.1.1.1"))
        self.store.write(create_audit_entry("auth", "logout", "user1", "1.1.1.1"))

        auth_logs = self.store.get_recent_logs(log_type="auth")
        self.assertEqual(len(auth_logs), 2)
        for log in auth_logs:
            self.assertEqual(log["log_type"], "auth")

    def test_get_recent_logs_filtered_by_user(self):
        """Test filtering recent logs by user."""
        self.store.write(create_audit_entry("auth", "login", "user1", "1.1.1.1"))
        self.store.write(create_audit_entry("auth", "login", "user2", "1.1.1.1"))
        self.store.write(create_audit_entry("auth", "logout", "user1", "1.1.1.1"))

        user1_logs = self.store.get_recent_logs(user_id="user1")
        self.assertEqual(len(user1_logs), 2)
        for log in user1_logs:
            self.assertEqual(log["user_id"], "user1")

    def test_query_logs_from_files(self):
        """Test querying logs from files."""
        # Write some entries
        for i in range(3):
            entry = create_audit_entry(
                log_type="auth",
                action=f"action_{i}",
                user_id="user123",
                ip_address="192.168.1.1",
            )
            self.store.write(entry)

        # Query logs
        results = self.store.query_logs("auth", limit=10)
        self.assertEqual(len(results), 3)


class TestAuditLogRetentionManager(unittest.TestCase):
    """Tests for AuditLogRetentionManager class."""

    def setUp(self):
        """Create temporary directory for tests."""
        self.temp_dir = tempfile.mkdtemp()
        self.store = AuditLogStore(log_dir=self.temp_dir)
        self.manager = AuditLogRetentionManager(
            log_store=self.store,
            cleanup_interval_hours=24,
        )

    def tearDown(self):
        """Clean up."""
        self.manager.stop()
        self.store.close()
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_start_and_stop(self):
        """Test starting and stopping the retention manager."""
        self.manager.start()
        self.assertTrue(self.manager._running)

        self.manager.stop()
        self.assertFalse(self.manager._running)

    def test_run_cleanup_empty_directory(self):
        """Test cleanup on empty directory."""
        stats = self.manager.run_cleanup()
        self.assertEqual(stats["archived_files"], 0)
        self.assertEqual(stats["deleted_files"], 0)
        self.assertEqual(stats["errors"], [])

    def test_get_retention_status(self):
        """Test getting retention status."""
        # Write some entries
        self.store.write(create_audit_entry("auth", "login", "user1", "1.1.1.1"))

        status = self.manager.get_retention_status()
        self.assertIn("policies", status)
        self.assertIn("log_files", status)
        self.assertIn("total_size_bytes", status)

    def test_retention_status_includes_policies(self):
        """Test that retention status includes policy information."""
        status = self.manager.get_retention_status()
        self.assertIn("auth", status["policies"])
        self.assertEqual(status["policies"]["auth"]["retention_days"], 90)


class TestAuditLogIntegration(unittest.TestCase):
    """Integration tests for audit logging system."""

    def setUp(self):
        """Create temporary directory for tests."""
        self.temp_dir = tempfile.mkdtemp()
        self.store = AuditLogStore(log_dir=self.temp_dir)

    def tearDown(self):
        """Clean up."""
        self.store.close()
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_full_audit_workflow(self):
        """Test complete audit logging workflow."""
        # 1. Write various audit entries
        self.store.write(create_audit_entry(
            "auth", "login", "admin", "10.0.0.1",
            details={"method": "password"},
        ))
        self.store.write(create_audit_entry(
            "order", "place_order", "admin", "10.0.0.1",
            details={"symbol": "BTCUSDT", "side": "BUY", "qty": 0.1},
        ))
        self.store.write(create_audit_entry(
            "auth", "logout", "admin", "10.0.0.1",
        ))

        # 2. Query recent logs
        recent = self.store.get_recent_logs(limit=10)
        self.assertEqual(len(recent), 3)

        # 3. Filter by type
        auth_logs = self.store.get_recent_logs(log_type="auth")
        self.assertEqual(len(auth_logs), 2)

        # 4. Query from files
        order_logs = self.store.query_logs("order")
        self.assertEqual(len(order_logs), 1)
        self.assertEqual(order_logs[0]["details"]["symbol"], "BTCUSDT")

    def test_log_file_creation(self):
        """Test that log files are created correctly."""
        self.store.write(create_audit_entry("auth", "login", "user1", "1.1.1.1"))
        self.store.write(create_audit_entry("order", "place", "user1", "1.1.1.1"))

        # Check files exist
        log_dir = Path(self.temp_dir)
        auth_files = list(log_dir.glob("auth_*.ndjson"))
        order_files = list(log_dir.glob("order_*.ndjson"))

        self.assertEqual(len(auth_files), 1)
        self.assertEqual(len(order_files), 1)


if __name__ == "__main__":
    unittest.main()
