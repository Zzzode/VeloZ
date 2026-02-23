"""Initial schema for VeloZ Gateway

Revision ID: 20260223_000001
Revises:
Create Date: 2026-02-23 00:00:01

This migration creates the initial database schema including:
- users: User accounts
- api_keys: API key management
- orders: Order tracking
- positions: Position management
- audit_logs: Audit trail
- trade_history: Trade execution history
- strategy_states: Strategy state persistence
"""
from typing import Sequence, Union

from alembic import op
import sqlalchemy as sa


# revision identifiers, used by Alembic.
revision: str = "20260223_000001"
down_revision: Union[str, None] = None
branch_labels: Union[str, Sequence[str], None] = None
depends_on: Union[str, Sequence[str], None] = None


def upgrade() -> None:
    """Create initial database schema."""

    # Users table
    op.create_table(
        "users",
        sa.Column("id", sa.String(64), primary_key=True),
        sa.Column("username", sa.String(255), unique=True, nullable=False),
        sa.Column("password_hash", sa.String(255), nullable=False),
        sa.Column("role", sa.String(50), nullable=False, server_default="viewer"),
        sa.Column("is_active", sa.Boolean(), nullable=False, server_default=sa.text("1")),
        sa.Column("created_at", sa.DateTime(), nullable=False, server_default=sa.func.now()),
        sa.Column("updated_at", sa.DateTime(), nullable=True),
        sa.Column("last_login_at", sa.DateTime(), nullable=True),
        sa.Column("failed_login_attempts", sa.Integer(), server_default="0"),
        sa.Column("locked_until", sa.DateTime(), nullable=True),
    )
    op.create_index("ix_users_username", "users", ["username"])
    op.create_index("ix_users_role", "users", ["role"])
    op.create_index("ix_users_is_active", "users", ["is_active"])

    # API Keys table
    op.create_table(
        "api_keys",
        sa.Column("id", sa.String(64), primary_key=True),
        sa.Column("user_id", sa.String(64), sa.ForeignKey("users.id", ondelete="CASCADE"), nullable=False),
        sa.Column("key_hash", sa.String(255), nullable=False, unique=True),
        sa.Column("name", sa.String(255), nullable=False),
        sa.Column("permissions", sa.JSON(), server_default="[]"),
        sa.Column("is_active", sa.Boolean(), nullable=False, server_default=sa.text("1")),
        sa.Column("created_at", sa.DateTime(), nullable=False, server_default=sa.func.now()),
        sa.Column("expires_at", sa.DateTime(), nullable=True),
        sa.Column("last_used_at", sa.DateTime(), nullable=True),
        sa.Column("usage_count", sa.Integer(), server_default="0"),
    )
    op.create_index("ix_api_keys_user_id", "api_keys", ["user_id"])
    op.create_index("ix_api_keys_is_active", "api_keys", ["is_active"])

    # Orders table
    op.create_table(
        "orders",
        sa.Column("id", sa.String(64), primary_key=True),
        sa.Column("client_order_id", sa.String(64), unique=True, nullable=False),
        sa.Column("exchange_order_id", sa.String(64), nullable=True),
        sa.Column("user_id", sa.String(64), sa.ForeignKey("users.id"), nullable=True),
        sa.Column("symbol", sa.String(20), nullable=False),
        sa.Column("side", sa.String(10), nullable=False),
        sa.Column("order_type", sa.String(20), nullable=False),
        sa.Column("quantity", sa.Float(), nullable=False),
        sa.Column("price", sa.Float(), nullable=True),
        sa.Column("stop_price", sa.Float(), nullable=True),
        sa.Column("time_in_force", sa.String(10), server_default="GTC"),
        sa.Column("status", sa.String(20), nullable=False, server_default="NEW"),
        sa.Column("filled_quantity", sa.Float(), server_default="0.0"),
        sa.Column("avg_fill_price", sa.Float(), nullable=True),
        sa.Column("commission", sa.Float(), server_default="0.0"),
        sa.Column("commission_asset", sa.String(10), nullable=True),
        sa.Column("exchange", sa.String(20), server_default="binance"),
        sa.Column("strategy_id", sa.String(64), nullable=True),
        sa.Column("created_at", sa.DateTime(), nullable=False, server_default=sa.func.now()),
        sa.Column("updated_at", sa.DateTime(), nullable=True),
        sa.Column("filled_at", sa.DateTime(), nullable=True),
        sa.Column("cancelled_at", sa.DateTime(), nullable=True),
        sa.Column("reject_reason", sa.Text(), nullable=True),
        sa.Column("metadata", sa.JSON(), server_default="{}"),
    )
    op.create_index("ix_orders_client_order_id", "orders", ["client_order_id"])
    op.create_index("ix_orders_exchange_order_id", "orders", ["exchange_order_id"])
    op.create_index("ix_orders_symbol", "orders", ["symbol"])
    op.create_index("ix_orders_status", "orders", ["status"])
    op.create_index("ix_orders_strategy_id", "orders", ["strategy_id"])
    op.create_index("ix_orders_symbol_status", "orders", ["symbol", "status"])
    op.create_index("ix_orders_created_at", "orders", ["created_at"])
    op.create_index("ix_orders_user_symbol", "orders", ["user_id", "symbol"])

    # Positions table
    op.create_table(
        "positions",
        sa.Column("id", sa.String(64), primary_key=True),
        sa.Column("user_id", sa.String(64), sa.ForeignKey("users.id"), nullable=True),
        sa.Column("symbol", sa.String(20), nullable=False),
        sa.Column("side", sa.String(10), nullable=False),
        sa.Column("quantity", sa.Float(), nullable=False),
        sa.Column("entry_price", sa.Float(), nullable=False),
        sa.Column("current_price", sa.Float(), nullable=True),
        sa.Column("unrealized_pnl", sa.Float(), server_default="0.0"),
        sa.Column("realized_pnl", sa.Float(), server_default="0.0"),
        sa.Column("leverage", sa.Float(), server_default="1.0"),
        sa.Column("margin_type", sa.String(20), server_default="CROSS"),
        sa.Column("liquidation_price", sa.Float(), nullable=True),
        sa.Column("exchange", sa.String(20), server_default="binance"),
        sa.Column("strategy_id", sa.String(64), nullable=True),
        sa.Column("opened_at", sa.DateTime(), nullable=False, server_default=sa.func.now()),
        sa.Column("updated_at", sa.DateTime(), nullable=True),
        sa.Column("closed_at", sa.DateTime(), nullable=True),
        sa.Column("is_open", sa.Boolean(), server_default=sa.text("1")),
        sa.Column("metadata", sa.JSON(), server_default="{}"),
    )
    op.create_index("ix_positions_symbol", "positions", ["symbol"])
    op.create_index("ix_positions_strategy_id", "positions", ["strategy_id"])
    op.create_index("ix_positions_is_open", "positions", ["is_open"])
    op.create_index("ix_positions_symbol_is_open", "positions", ["symbol", "is_open"])
    op.create_index("ix_positions_user_symbol", "positions", ["user_id", "symbol"])

    # Audit Logs table
    op.create_table(
        "audit_logs",
        sa.Column("id", sa.BigInteger(), primary_key=True, autoincrement=True),
        sa.Column("timestamp", sa.DateTime(), nullable=False, server_default=sa.func.now()),
        sa.Column("log_type", sa.String(50), nullable=False),
        sa.Column("action", sa.String(100), nullable=False),
        sa.Column("user_id", sa.String(64), nullable=True),
        sa.Column("ip_address", sa.String(45), nullable=True),
        sa.Column("request_id", sa.String(64), nullable=True),
        sa.Column("resource_type", sa.String(50), nullable=True),
        sa.Column("resource_id", sa.String(64), nullable=True),
        sa.Column("old_value", sa.JSON(), nullable=True),
        sa.Column("new_value", sa.JSON(), nullable=True),
        sa.Column("details", sa.JSON(), server_default="{}"),
        sa.Column("status", sa.String(20), server_default="success"),
        sa.Column("error_message", sa.Text(), nullable=True),
        sa.Column("duration_ms", sa.Integer(), nullable=True),
    )
    op.create_index("ix_audit_logs_timestamp", "audit_logs", ["timestamp"])
    op.create_index("ix_audit_logs_log_type", "audit_logs", ["log_type"])
    op.create_index("ix_audit_logs_action", "audit_logs", ["action"])
    op.create_index("ix_audit_logs_user_id", "audit_logs", ["user_id"])
    op.create_index("ix_audit_logs_request_id", "audit_logs", ["request_id"])
    op.create_index("ix_audit_logs_type_action", "audit_logs", ["log_type", "action"])
    op.create_index("ix_audit_logs_timestamp_type", "audit_logs", ["timestamp", "log_type"])
    op.create_index("ix_audit_logs_user_timestamp", "audit_logs", ["user_id", "timestamp"])

    # Trade History table
    op.create_table(
        "trade_history",
        sa.Column("id", sa.BigInteger(), primary_key=True, autoincrement=True),
        sa.Column("order_id", sa.String(64), sa.ForeignKey("orders.id"), nullable=False),
        sa.Column("trade_id", sa.String(64), nullable=True, unique=True),
        sa.Column("symbol", sa.String(20), nullable=False),
        sa.Column("side", sa.String(10), nullable=False),
        sa.Column("price", sa.Float(), nullable=False),
        sa.Column("quantity", sa.Float(), nullable=False),
        sa.Column("quote_quantity", sa.Float(), nullable=True),
        sa.Column("commission", sa.Float(), server_default="0.0"),
        sa.Column("commission_asset", sa.String(10), nullable=True),
        sa.Column("is_maker", sa.Boolean(), server_default=sa.text("0")),
        sa.Column("executed_at", sa.DateTime(), nullable=False, server_default=sa.func.now()),
    )
    op.create_index("ix_trade_history_order_id", "trade_history", ["order_id"])
    op.create_index("ix_trade_history_symbol", "trade_history", ["symbol"])
    op.create_index("ix_trade_history_executed_at", "trade_history", ["executed_at"])
    op.create_index("ix_trade_history_symbol_executed", "trade_history", ["symbol", "executed_at"])

    # Strategy States table
    op.create_table(
        "strategy_states",
        sa.Column("id", sa.String(64), primary_key=True),
        sa.Column("strategy_type", sa.String(50), nullable=False),
        sa.Column("name", sa.String(255), nullable=False),
        sa.Column("config", sa.JSON(), server_default="{}"),
        sa.Column("state", sa.JSON(), server_default="{}"),
        sa.Column("is_active", sa.Boolean(), server_default=sa.text("1")),
        sa.Column("created_at", sa.DateTime(), nullable=False, server_default=sa.func.now()),
        sa.Column("updated_at", sa.DateTime(), nullable=True),
        sa.Column("last_signal_at", sa.DateTime(), nullable=True),
        sa.Column("performance_metrics", sa.JSON(), server_default="{}"),
    )
    op.create_index("ix_strategy_states_strategy_type", "strategy_states", ["strategy_type"])
    op.create_index("ix_strategy_states_is_active", "strategy_states", ["is_active"])
    op.create_index("ix_strategy_states_type_active", "strategy_states", ["strategy_type", "is_active"])


def downgrade() -> None:
    """Drop all tables."""
    op.drop_table("trade_history")
    op.drop_table("strategy_states")
    op.drop_table("audit_logs")
    op.drop_table("positions")
    op.drop_table("orders")
    op.drop_table("api_keys")
    op.drop_table("users")
