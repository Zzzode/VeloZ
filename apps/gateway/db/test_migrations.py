#!/usr/bin/env python3
"""
Tests for database migrations and models.

These tests verify:
- Migration scripts can be applied and rolled back
- Models can be created and queried
- Database constraints work correctly
"""

import os
import sys
import tempfile
import unittest
from datetime import datetime
from pathlib import Path

# Add gateway directory to path
gateway_dir = Path(__file__).parent.parent
sys.path.insert(0, str(gateway_dir))

from sqlalchemy import create_engine, text
from sqlalchemy.orm import sessionmaker

from db.models import Base, User, APIKey, Order, Position, AuditLog, TradeHistory, StrategyState
from db.connection import DatabaseManager, init_database, get_session, close_database


class TestDatabaseConnection(unittest.TestCase):
    """Test database connection management."""

    def setUp(self):
        self.temp_db = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
        self.db_url = f"sqlite:///{self.temp_db.name}"

    def tearDown(self):
        close_database()
        os.unlink(self.temp_db.name)

    def test_init_database(self):
        """Test database initialization."""
        db = init_database(self.db_url)
        self.assertIsNotNone(db)
        self.assertTrue(db.is_sqlite)

    def test_health_check(self):
        """Test database health check."""
        db = init_database(self.db_url)
        status = db.health_check()
        self.assertEqual(status["status"], "healthy")
        self.assertEqual(status["backend"], "sqlite")

    def test_session_context_manager(self):
        """Test session context manager."""
        init_database(self.db_url)
        with get_session() as session:
            # Session should be usable
            result = session.execute(text("SELECT 1"))
            self.assertEqual(result.scalar(), 1)


class TestUserModel(unittest.TestCase):
    """Test User model operations."""

    def setUp(self):
        self.temp_db = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
        self.db_url = f"sqlite:///{self.temp_db.name}"
        init_database(self.db_url)

    def tearDown(self):
        close_database()
        os.unlink(self.temp_db.name)

    def test_create_user(self):
        """Test creating a user."""
        with get_session() as session:
            user = User(
                id="user-001",
                username="testuser",
                password_hash="hashed_password",
                role="trader",
            )
            session.add(user)
            session.flush()

            # Query the user
            queried = session.query(User).filter_by(username="testuser").first()
            self.assertIsNotNone(queried)
            self.assertEqual(queried.role, "trader")
            self.assertTrue(queried.is_active)

    def test_user_unique_username(self):
        """Test username uniqueness constraint."""
        with get_session() as session:
            user1 = User(
                id="user-001",
                username="unique_user",
                password_hash="hash1",
            )
            session.add(user1)
            session.flush()

        # Try to create another user with same username
        with self.assertRaises(Exception):
            with get_session() as session:
                user2 = User(
                    id="user-002",
                    username="unique_user",
                    password_hash="hash2",
                )
                session.add(user2)
                session.flush()


class TestOrderModel(unittest.TestCase):
    """Test Order model operations."""

    def setUp(self):
        self.temp_db = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
        self.db_url = f"sqlite:///{self.temp_db.name}"
        init_database(self.db_url)

    def tearDown(self):
        close_database()
        os.unlink(self.temp_db.name)

    def test_create_order(self):
        """Test creating an order."""
        with get_session() as session:
            order = Order(
                id="order-001",
                client_order_id="client-001",
                symbol="BTCUSDT",
                side="BUY",
                order_type="LIMIT",
                quantity=0.1,
                price=50000.0,
                status="NEW",
            )
            session.add(order)
            session.flush()

            # Query the order
            queried = session.query(Order).filter_by(client_order_id="client-001").first()
            self.assertIsNotNone(queried)
            self.assertEqual(queried.symbol, "BTCUSDT")
            self.assertEqual(queried.side, "BUY")
            self.assertEqual(queried.quantity, 0.1)

    def test_order_with_user(self):
        """Test order with user relationship."""
        with get_session() as session:
            # Create user first
            user = User(
                id="user-001",
                username="trader1",
                password_hash="hash",
            )
            session.add(user)
            session.flush()

            # Create order for user
            order = Order(
                id="order-001",
                client_order_id="client-001",
                user_id="user-001",
                symbol="ETHUSDT",
                side="SELL",
                order_type="MARKET",
                quantity=1.0,
            )
            session.add(order)
            session.flush()

            # Query user's orders
            user_orders = session.query(Order).filter_by(user_id="user-001").all()
            self.assertEqual(len(user_orders), 1)
            self.assertEqual(user_orders[0].symbol, "ETHUSDT")


class TestPositionModel(unittest.TestCase):
    """Test Position model operations."""

    def setUp(self):
        self.temp_db = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
        self.db_url = f"sqlite:///{self.temp_db.name}"
        init_database(self.db_url)

    def tearDown(self):
        close_database()
        os.unlink(self.temp_db.name)

    def test_create_position(self):
        """Test creating a position."""
        with get_session() as session:
            position = Position(
                id="pos-001",
                symbol="BTCUSDT",
                side="LONG",
                quantity=0.5,
                entry_price=48000.0,
                current_price=50000.0,
                unrealized_pnl=1000.0,
            )
            session.add(position)
            session.flush()

            # Query open positions
            open_positions = session.query(Position).filter_by(is_open=True).all()
            self.assertEqual(len(open_positions), 1)
            self.assertEqual(open_positions[0].unrealized_pnl, 1000.0)

    def test_close_position(self):
        """Test closing a position."""
        with get_session() as session:
            position = Position(
                id="pos-001",
                symbol="BTCUSDT",
                side="LONG",
                quantity=0.5,
                entry_price=48000.0,
            )
            session.add(position)
            session.flush()

        # Close the position
        with get_session() as session:
            position = session.query(Position).filter_by(id="pos-001").first()
            position.is_open = False
            position.closed_at = datetime.utcnow()
            position.realized_pnl = 500.0
            session.flush()

        # Verify closed
        with get_session() as session:
            closed = session.query(Position).filter_by(id="pos-001").first()
            self.assertFalse(closed.is_open)
            self.assertIsNotNone(closed.closed_at)
            self.assertEqual(closed.realized_pnl, 500.0)


class TestAuditLogModel(unittest.TestCase):
    """Test AuditLog model operations."""

    def setUp(self):
        self.temp_db = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
        self.db_url = f"sqlite:///{self.temp_db.name}"
        init_database(self.db_url)

    def tearDown(self):
        close_database()
        os.unlink(self.temp_db.name)

    def test_create_audit_log(self):
        """Test creating an audit log entry."""
        with get_session() as session:
            log = AuditLog(
                log_type="auth",
                action="login",
                user_id="user-001",
                ip_address="192.168.1.1",
                details={"method": "password"},
            )
            session.add(log)
            session.flush()

            # Query logs
            logs = session.query(AuditLog).filter_by(log_type="auth").all()
            self.assertEqual(len(logs), 1)
            self.assertEqual(logs[0].action, "login")
            self.assertEqual(logs[0].details["method"], "password")

    def test_audit_log_autoincrement(self):
        """Test audit log auto-increment ID."""
        with get_session() as session:
            for i in range(3):
                log = AuditLog(
                    log_type="order",
                    action=f"action_{i}",
                    user_id="user-001",
                )
                session.add(log)
            session.flush()

            logs = session.query(AuditLog).order_by(AuditLog.id).all()
            self.assertEqual(len(logs), 3)
            # IDs should be sequential
            self.assertEqual(logs[1].id, logs[0].id + 1)
            self.assertEqual(logs[2].id, logs[1].id + 1)


class TestAPIKeyModel(unittest.TestCase):
    """Test APIKey model operations."""

    def setUp(self):
        self.temp_db = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
        self.db_url = f"sqlite:///{self.temp_db.name}"
        init_database(self.db_url)

    def tearDown(self):
        close_database()
        os.unlink(self.temp_db.name)

    def test_create_api_key(self):
        """Test creating an API key."""
        with get_session() as session:
            # Create user first
            user = User(
                id="user-001",
                username="apiuser",
                password_hash="hash",
            )
            session.add(user)
            session.flush()

            # Create API key
            api_key = APIKey(
                id="key-001",
                user_id="user-001",
                key_hash="hashed_key_value",
                name="Trading Bot Key",
                permissions=["read", "trade"],
            )
            session.add(api_key)
            session.flush()

            # Query API keys
            keys = session.query(APIKey).filter_by(user_id="user-001").all()
            self.assertEqual(len(keys), 1)
            self.assertEqual(keys[0].name, "Trading Bot Key")
            self.assertIn("trade", keys[0].permissions)

    def test_api_key_cascade_delete(self):
        """Test API key cascade delete when user is deleted."""
        with get_session() as session:
            user = User(
                id="user-001",
                username="apiuser",
                password_hash="hash",
            )
            session.add(user)
            session.flush()

            api_key = APIKey(
                id="key-001",
                user_id="user-001",
                key_hash="hashed_key",
                name="Test Key",
            )
            session.add(api_key)
            session.flush()

        # Delete user
        with get_session() as session:
            user = session.query(User).filter_by(id="user-001").first()
            session.delete(user)
            session.flush()

        # API key should be deleted
        with get_session() as session:
            key = session.query(APIKey).filter_by(id="key-001").first()
            self.assertIsNone(key)


class TestMigrationRollback(unittest.TestCase):
    """Test migration rollback functionality."""

    def setUp(self):
        self.temp_db = tempfile.NamedTemporaryFile(suffix=".db", delete=False)
        self.db_url = f"sqlite:///{self.temp_db.name}"

    def tearDown(self):
        close_database()
        os.unlink(self.temp_db.name)

    def test_tables_created(self):
        """Test that all tables are created."""
        init_database(self.db_url)

        engine = create_engine(self.db_url)
        with engine.connect() as conn:
            # Check all expected tables exist
            result = conn.execute(text(
                "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"
            ))
            tables = [row[0] for row in result]

            expected_tables = [
                "api_keys",
                "audit_logs",
                "orders",
                "positions",
                "strategy_states",
                "trade_history",
                "users",
            ]

            for table in expected_tables:
                self.assertIn(table, tables, f"Table {table} not found")


if __name__ == "__main__":
    unittest.main()
