"""
Database connection management for VeloZ Gateway.

Supports SQLite (development) and PostgreSQL (production) backends.
"""

import os
import logging
from contextlib import contextmanager
from typing import Optional, Generator
from sqlalchemy import create_engine, event, text
from sqlalchemy.orm import sessionmaker, Session
from sqlalchemy.pool import QueuePool, StaticPool

from .models import Base

logger = logging.getLogger("veloz.db")


class DatabaseManager:
    """
    Manages database connections and sessions.

    Supports:
    - SQLite for development/testing
    - PostgreSQL for production
    """

    def __init__(
        self,
        database_url: Optional[str] = None,
        echo: bool = False,
        pool_size: int = 5,
        max_overflow: int = 10,
    ):
        """
        Initialize database manager.

        Args:
            database_url: Database connection URL. Defaults to SQLite if not provided.
            echo: Enable SQL query logging.
            pool_size: Connection pool size (PostgreSQL only).
            max_overflow: Max overflow connections (PostgreSQL only).
        """
        self._database_url = database_url or os.getenv(
            "VELOZ_DATABASE_URL",
            "sqlite:///veloz_gateway.db"
        )
        self._echo = echo
        self._pool_size = pool_size
        self._max_overflow = max_overflow
        self._engine = None
        self._session_factory = None

    @property
    def is_sqlite(self) -> bool:
        """Check if using SQLite backend."""
        return self._database_url.startswith("sqlite")

    @property
    def is_postgresql(self) -> bool:
        """Check if using PostgreSQL backend."""
        return self._database_url.startswith("postgresql")

    def connect(self) -> None:
        """Establish database connection and create engine."""
        if self._engine is not None:
            return

        engine_kwargs = {
            "echo": self._echo,
        }

        if self.is_sqlite:
            # SQLite-specific configuration
            engine_kwargs["connect_args"] = {"check_same_thread": False}
            engine_kwargs["poolclass"] = StaticPool
        else:
            # PostgreSQL configuration
            engine_kwargs["poolclass"] = QueuePool
            engine_kwargs["pool_size"] = self._pool_size
            engine_kwargs["max_overflow"] = self._max_overflow
            engine_kwargs["pool_pre_ping"] = True
            # SSL configuration for production
            ssl_mode = os.getenv("VELOZ_DB_SSL_MODE", "prefer")
            if ssl_mode != "disable":
                engine_kwargs["connect_args"] = {"sslmode": ssl_mode}

        self._engine = create_engine(self._database_url, **engine_kwargs)

        # Enable foreign keys for SQLite
        if self.is_sqlite:
            @event.listens_for(self._engine, "connect")
            def set_sqlite_pragma(dbapi_connection, connection_record):
                cursor = dbapi_connection.cursor()
                cursor.execute("PRAGMA foreign_keys=ON")
                cursor.close()

        self._session_factory = sessionmaker(
            bind=self._engine,
            autocommit=False,
            autoflush=False,
        )

        logger.info(f"Database connected: {self._database_url.split('@')[-1] if '@' in self._database_url else self._database_url}")

    def create_tables(self) -> None:
        """Create all tables defined in models."""
        if self._engine is None:
            self.connect()
        Base.metadata.create_all(self._engine)
        logger.info("Database tables created")

    def drop_tables(self) -> None:
        """Drop all tables (use with caution)."""
        if self._engine is None:
            self.connect()
        Base.metadata.drop_all(self._engine)
        logger.warning("Database tables dropped")

    @contextmanager
    def session(self) -> Generator[Session, None, None]:
        """
        Provide a transactional scope around a series of operations.

        Usage:
            with db_manager.session() as session:
                session.add(obj)
                session.commit()
        """
        if self._session_factory is None:
            self.connect()

        session = self._session_factory()
        try:
            yield session
            session.commit()
        except Exception:
            session.rollback()
            raise
        finally:
            session.close()

    def get_session(self) -> Session:
        """Get a new session (caller must manage lifecycle)."""
        if self._session_factory is None:
            self.connect()
        return self._session_factory()

    def close(self) -> None:
        """Close database connection."""
        if self._engine is not None:
            self._engine.dispose()
            self._engine = None
            self._session_factory = None
            logger.info("Database connection closed")

    def health_check(self) -> dict:
        """Check database connectivity and return status."""
        try:
            with self.session() as session:
                if self.is_sqlite:
                    session.execute(text("SELECT 1"))
                else:
                    session.execute(text("SELECT 1"))
            return {
                "status": "healthy",
                "backend": "sqlite" if self.is_sqlite else "postgresql",
                "url": self._database_url.split("@")[-1] if "@" in self._database_url else self._database_url,
            }
        except Exception as e:
            return {
                "status": "unhealthy",
                "error": str(e),
            }


# Global database manager instance
_db_manager: Optional[DatabaseManager] = None


def init_database(
    database_url: Optional[str] = None,
    echo: bool = False,
    create_tables: bool = True,
) -> DatabaseManager:
    """
    Initialize the global database manager.

    Args:
        database_url: Database connection URL.
        echo: Enable SQL query logging.
        create_tables: Create tables if they don't exist.

    Returns:
        DatabaseManager instance.
    """
    global _db_manager

    if _db_manager is not None:
        _db_manager.close()

    _db_manager = DatabaseManager(database_url=database_url, echo=echo)
    _db_manager.connect()

    if create_tables:
        _db_manager.create_tables()

    return _db_manager


def get_db_manager() -> Optional[DatabaseManager]:
    """Get the global database manager instance."""
    return _db_manager


@contextmanager
def get_session() -> Generator[Session, None, None]:
    """Get a database session from the global manager."""
    if _db_manager is None:
        raise RuntimeError("Database not initialized. Call init_database() first.")
    with _db_manager.session() as session:
        yield session


def close_database() -> None:
    """Close the global database connection."""
    global _db_manager
    if _db_manager is not None:
        _db_manager.close()
        _db_manager = None
