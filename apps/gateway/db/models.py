"""
SQLAlchemy ORM models for VeloZ Gateway.

These models define the database schema for persistent storage of:
- Orders and order history
- Positions
- Audit logs
- API keys
- Users
"""

from datetime import datetime
from decimal import Decimal
from typing import Optional
from sqlalchemy import (
    Column,
    String,
    Integer,
    Float,
    Boolean,
    DateTime,
    Text,
    Index,
    Enum,
    ForeignKey,
    JSON,
    BigInteger,
)
from sqlalchemy.orm import declarative_base, relationship

Base = declarative_base()


class User(Base):
    """User account model."""

    __tablename__ = "users"

    id = Column(String(64), primary_key=True)
    username = Column(String(255), unique=True, nullable=False, index=True)
    password_hash = Column(String(255), nullable=False)
    role = Column(String(50), nullable=False, default="viewer")
    is_active = Column(Boolean, default=True, nullable=False)
    created_at = Column(DateTime, default=datetime.utcnow, nullable=False)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    last_login_at = Column(DateTime, nullable=True)
    failed_login_attempts = Column(Integer, default=0)
    locked_until = Column(DateTime, nullable=True)

    # Relationships
    api_keys = relationship("APIKey", back_populates="user", cascade="all, delete-orphan")
    orders = relationship("Order", back_populates="user")

    __table_args__ = (
        Index("ix_users_role", "role"),
        Index("ix_users_is_active", "is_active"),
    )


class APIKey(Base):
    """API key model for programmatic access."""

    __tablename__ = "api_keys"

    id = Column(String(64), primary_key=True)
    user_id = Column(String(64), ForeignKey("users.id", ondelete="CASCADE"), nullable=False)
    key_hash = Column(String(255), nullable=False, unique=True)
    name = Column(String(255), nullable=False)
    permissions = Column(JSON, default=list)
    is_active = Column(Boolean, default=True, nullable=False)
    created_at = Column(DateTime, default=datetime.utcnow, nullable=False)
    expires_at = Column(DateTime, nullable=True)
    last_used_at = Column(DateTime, nullable=True)
    usage_count = Column(Integer, default=0)

    # Relationships
    user = relationship("User", back_populates="api_keys")

    __table_args__ = (
        Index("ix_api_keys_user_id", "user_id"),
        Index("ix_api_keys_is_active", "is_active"),
    )


class Order(Base):
    """Order model for tracking order lifecycle."""

    __tablename__ = "orders"

    id = Column(String(64), primary_key=True)
    client_order_id = Column(String(64), unique=True, nullable=False, index=True)
    exchange_order_id = Column(String(64), nullable=True, index=True)
    user_id = Column(String(64), ForeignKey("users.id"), nullable=True)
    symbol = Column(String(20), nullable=False, index=True)
    side = Column(String(10), nullable=False)  # BUY, SELL
    order_type = Column(String(20), nullable=False)  # LIMIT, MARKET, STOP_LIMIT
    quantity = Column(Float, nullable=False)
    price = Column(Float, nullable=True)
    stop_price = Column(Float, nullable=True)
    time_in_force = Column(String(10), default="GTC")  # GTC, IOC, FOK
    status = Column(String(20), nullable=False, default="NEW", index=True)
    filled_quantity = Column(Float, default=0.0)
    avg_fill_price = Column(Float, nullable=True)
    commission = Column(Float, default=0.0)
    commission_asset = Column(String(10), nullable=True)
    exchange = Column(String(20), default="binance")
    strategy_id = Column(String(64), nullable=True, index=True)
    created_at = Column(DateTime, default=datetime.utcnow, nullable=False)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    filled_at = Column(DateTime, nullable=True)
    cancelled_at = Column(DateTime, nullable=True)
    reject_reason = Column(Text, nullable=True)
    metadata = Column(JSON, default=dict)

    # Relationships
    user = relationship("User", back_populates="orders")

    __table_args__ = (
        Index("ix_orders_symbol_status", "symbol", "status"),
        Index("ix_orders_created_at", "created_at"),
        Index("ix_orders_user_symbol", "user_id", "symbol"),
    )


class Position(Base):
    """Position model for tracking current holdings."""

    __tablename__ = "positions"

    id = Column(String(64), primary_key=True)
    user_id = Column(String(64), ForeignKey("users.id"), nullable=True)
    symbol = Column(String(20), nullable=False, index=True)
    side = Column(String(10), nullable=False)  # LONG, SHORT
    quantity = Column(Float, nullable=False)
    entry_price = Column(Float, nullable=False)
    current_price = Column(Float, nullable=True)
    unrealized_pnl = Column(Float, default=0.0)
    realized_pnl = Column(Float, default=0.0)
    leverage = Column(Float, default=1.0)
    margin_type = Column(String(20), default="CROSS")  # CROSS, ISOLATED
    liquidation_price = Column(Float, nullable=True)
    exchange = Column(String(20), default="binance")
    strategy_id = Column(String(64), nullable=True, index=True)
    opened_at = Column(DateTime, default=datetime.utcnow, nullable=False)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    closed_at = Column(DateTime, nullable=True)
    is_open = Column(Boolean, default=True, index=True)
    metadata = Column(JSON, default=dict)

    __table_args__ = (
        Index("ix_positions_symbol_is_open", "symbol", "is_open"),
        Index("ix_positions_user_symbol", "user_id", "symbol"),
    )


class AuditLog(Base):
    """Audit log model for compliance and debugging."""

    __tablename__ = "audit_logs"

    id = Column(BigInteger, primary_key=True, autoincrement=True)
    timestamp = Column(DateTime, default=datetime.utcnow, nullable=False, index=True)
    log_type = Column(String(50), nullable=False, index=True)
    action = Column(String(100), nullable=False, index=True)
    user_id = Column(String(64), nullable=True, index=True)
    ip_address = Column(String(45), nullable=True)  # IPv6 max length
    request_id = Column(String(64), nullable=True, index=True)
    resource_type = Column(String(50), nullable=True)
    resource_id = Column(String(64), nullable=True)
    old_value = Column(JSON, nullable=True)
    new_value = Column(JSON, nullable=True)
    details = Column(JSON, default=dict)
    status = Column(String(20), default="success")  # success, failure, error
    error_message = Column(Text, nullable=True)
    duration_ms = Column(Integer, nullable=True)

    __table_args__ = (
        Index("ix_audit_logs_type_action", "log_type", "action"),
        Index("ix_audit_logs_timestamp_type", "timestamp", "log_type"),
        Index("ix_audit_logs_user_timestamp", "user_id", "timestamp"),
    )


class TradeHistory(Base):
    """Trade execution history for analytics."""

    __tablename__ = "trade_history"

    id = Column(BigInteger, primary_key=True, autoincrement=True)
    order_id = Column(String(64), ForeignKey("orders.id"), nullable=False, index=True)
    trade_id = Column(String(64), nullable=True, unique=True)
    symbol = Column(String(20), nullable=False, index=True)
    side = Column(String(10), nullable=False)
    price = Column(Float, nullable=False)
    quantity = Column(Float, nullable=False)
    quote_quantity = Column(Float, nullable=True)
    commission = Column(Float, default=0.0)
    commission_asset = Column(String(10), nullable=True)
    is_maker = Column(Boolean, default=False)
    executed_at = Column(DateTime, default=datetime.utcnow, nullable=False, index=True)

    __table_args__ = (
        Index("ix_trade_history_symbol_executed", "symbol", "executed_at"),
    )


class StrategyState(Base):
    """Strategy state persistence for recovery."""

    __tablename__ = "strategy_states"

    id = Column(String(64), primary_key=True)
    strategy_type = Column(String(50), nullable=False, index=True)
    name = Column(String(255), nullable=False)
    config = Column(JSON, default=dict)
    state = Column(JSON, default=dict)
    is_active = Column(Boolean, default=True, index=True)
    created_at = Column(DateTime, default=datetime.utcnow, nullable=False)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    last_signal_at = Column(DateTime, nullable=True)
    performance_metrics = Column(JSON, default=dict)

    __table_args__ = (
        Index("ix_strategy_states_type_active", "strategy_type", "is_active"),
    )
