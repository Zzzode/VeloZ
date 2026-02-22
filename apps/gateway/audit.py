#!/usr/bin/env python3
"""
Audit log retention policies and management for VeloZ gateway.

This module provides:
- Configurable retention periods by log type
- Scheduled cleanup jobs
- Log archiving before deletion
- File-based audit log storage with rotation
"""

import json
import gzip
import os
import shutil
import threading
import time
import logging
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from pathlib import Path
from typing import Optional, Callable
from collections import deque

# Configure module logger
logger = logging.getLogger("veloz.audit.retention")


@dataclass
class RetentionPolicy:
    """Retention policy for a specific log type."""
    log_type: str
    retention_days: int = 30
    archive_before_delete: bool = True
    max_file_size_mb: int = 100
    max_files: int = 10


@dataclass
class AuditLogEntry:
    """Single audit log entry."""
    timestamp: float
    log_type: str
    action: str
    user_id: str
    ip_address: str
    details: dict = field(default_factory=dict)
    request_id: Optional[str] = None

    def to_dict(self) -> dict:
        return {
            "timestamp": self.timestamp,
            "datetime": datetime.fromtimestamp(self.timestamp).isoformat(),
            "log_type": self.log_type,
            "action": self.action,
            "user_id": self.user_id,
            "ip_address": self.ip_address,
            "details": self.details,
            "request_id": self.request_id,
        }

    def to_json(self) -> str:
        return json.dumps(self.to_dict())


# Default retention policies by log type
DEFAULT_POLICIES = {
    "auth": RetentionPolicy(
        log_type="auth",
        retention_days=90,  # Keep auth logs for 90 days
        archive_before_delete=True,
    ),
    "order": RetentionPolicy(
        log_type="order",
        retention_days=365,  # Keep order logs for 1 year
        archive_before_delete=True,
    ),
    "api_key": RetentionPolicy(
        log_type="api_key",
        retention_days=365,  # Keep API key logs for 1 year
        archive_before_delete=True,
    ),
    "error": RetentionPolicy(
        log_type="error",
        retention_days=30,  # Keep error logs for 30 days
        archive_before_delete=True,
    ),
    "access": RetentionPolicy(
        log_type="access",
        retention_days=14,  # Keep access logs for 14 days
        archive_before_delete=False,
    ),
    "default": RetentionPolicy(
        log_type="default",
        retention_days=30,
        archive_before_delete=True,
    ),
}


class AuditLogStore:
    """
    File-based audit log storage with rotation and retention.

    Logs are stored in NDJSON format (newline-delimited JSON) for easy parsing.
    """

    def __init__(
        self,
        log_dir: str = "/var/log/veloz/audit",
        policies: Optional[dict[str, RetentionPolicy]] = None,
    ):
        self._log_dir = Path(log_dir)
        self._log_dir.mkdir(parents=True, exist_ok=True)
        self._archive_dir = self._log_dir / "archive"
        self._archive_dir.mkdir(parents=True, exist_ok=True)

        self._policies = policies or DEFAULT_POLICIES.copy()
        self._mu = threading.Lock()

        # In-memory buffer for recent logs (for quick access)
        self._recent_logs: deque[AuditLogEntry] = deque(maxlen=10000)

        # File handles for each log type
        self._file_handles: dict[str, tuple] = {}  # log_type -> (file, path, size)

    def get_policy(self, log_type: str) -> RetentionPolicy:
        """Get retention policy for a log type."""
        return self._policies.get(log_type, self._policies.get("default", DEFAULT_POLICIES["default"]))

    def set_policy(self, policy: RetentionPolicy) -> None:
        """Set or update a retention policy."""
        with self._mu:
            self._policies[policy.log_type] = policy
        logger.info(f"Updated retention policy for {policy.log_type}: {policy.retention_days} days")

    def write(self, entry: AuditLogEntry) -> None:
        """Write an audit log entry."""
        with self._mu:
            # Add to in-memory buffer
            self._recent_logs.append(entry)

            # Write to file
            self._write_to_file(entry)

    def _get_log_file_path(self, log_type: str) -> Path:
        """Get the current log file path for a log type."""
        date_str = datetime.now().strftime("%Y-%m-%d")
        return self._log_dir / f"{log_type}_{date_str}.ndjson"

    def _write_to_file(self, entry: AuditLogEntry) -> None:
        """Write entry to the appropriate log file."""
        log_type = entry.log_type
        policy = self.get_policy(log_type)
        file_path = self._get_log_file_path(log_type)

        # Check if we need to rotate
        if file_path.exists():
            file_size_mb = file_path.stat().st_size / (1024 * 1024)
            if file_size_mb >= policy.max_file_size_mb:
                self._rotate_file(log_type, file_path)

        # Write the entry
        try:
            with open(file_path, "a", encoding="utf-8") as f:
                f.write(entry.to_json() + "\n")
        except Exception as e:
            logger.error(f"Failed to write audit log: {e}")

    def _rotate_file(self, log_type: str, file_path: Path) -> None:
        """Rotate a log file when it exceeds size limit."""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        rotated_name = f"{log_type}_{timestamp}.ndjson"
        rotated_path = self._log_dir / rotated_name

        try:
            shutil.move(str(file_path), str(rotated_path))
            logger.info(f"Rotated log file: {file_path} -> {rotated_path}")
        except Exception as e:
            logger.error(f"Failed to rotate log file: {e}")

    def get_recent_logs(
        self,
        log_type: Optional[str] = None,
        limit: int = 100,
        user_id: Optional[str] = None,
    ) -> list[dict]:
        """Get recent audit logs from memory."""
        with self._mu:
            logs = list(self._recent_logs)

        # Filter
        if log_type:
            logs = [l for l in logs if l.log_type == log_type]
        if user_id:
            logs = [l for l in logs if l.user_id == user_id]

        # Sort by timestamp descending and limit
        logs.sort(key=lambda x: x.timestamp, reverse=True)
        return [l.to_dict() for l in logs[:limit]]

    def query_logs(
        self,
        log_type: str,
        start_time: Optional[datetime] = None,
        end_time: Optional[datetime] = None,
        user_id: Optional[str] = None,
        action: Optional[str] = None,
        limit: int = 1000,
    ) -> list[dict]:
        """Query logs from files."""
        results = []
        start_ts = start_time.timestamp() if start_time else 0
        end_ts = end_time.timestamp() if end_time else time.time()

        # Find relevant log files
        pattern = f"{log_type}_*.ndjson"
        for log_file in sorted(self._log_dir.glob(pattern), reverse=True):
            if len(results) >= limit:
                break

            try:
                with open(log_file, "r", encoding="utf-8") as f:
                    for line in f:
                        if len(results) >= limit:
                            break
                        try:
                            entry = json.loads(line.strip())
                            ts = entry.get("timestamp", 0)
                            if ts < start_ts or ts > end_ts:
                                continue
                            if user_id and entry.get("user_id") != user_id:
                                continue
                            if action and entry.get("action") != action:
                                continue
                            results.append(entry)
                        except json.JSONDecodeError:
                            continue
            except Exception as e:
                logger.error(f"Error reading log file {log_file}: {e}")

        return results

    def close(self) -> None:
        """Close all file handles."""
        with self._mu:
            for log_type, (fh, _, _) in self._file_handles.items():
                try:
                    fh.close()
                except Exception:
                    pass
            self._file_handles.clear()


class AuditLogRetentionManager:
    """
    Manages audit log retention and cleanup.

    Runs periodic cleanup jobs to:
    - Archive old logs
    - Delete logs past retention period
    - Manage disk space
    """

    def __init__(
        self,
        log_store: AuditLogStore,
        cleanup_interval_hours: int = 24,
    ):
        self._store = log_store
        self._cleanup_interval = cleanup_interval_hours * 3600
        self._running = False
        self._cleanup_thread: Optional[threading.Thread] = None
        self._stop_event = threading.Event()

    def start(self) -> None:
        """Start the retention manager background thread."""
        if self._running:
            return

        self._running = True
        self._stop_event.clear()
        self._cleanup_thread = threading.Thread(target=self._cleanup_loop, daemon=True)
        self._cleanup_thread.start()
        logger.info("Audit log retention manager started")

    def stop(self) -> None:
        """Stop the retention manager."""
        if not self._running:
            return

        self._running = False
        self._stop_event.set()
        if self._cleanup_thread:
            self._cleanup_thread.join(timeout=5.0)
        logger.info("Audit log retention manager stopped")

    def _cleanup_loop(self) -> None:
        """Background cleanup loop."""
        while self._running:
            try:
                self.run_cleanup()
            except Exception as e:
                logger.error(f"Cleanup error: {e}")

            # Wait for next cleanup or stop signal
            self._stop_event.wait(timeout=self._cleanup_interval)

    def run_cleanup(self) -> dict:
        """
        Run cleanup for all log types.
        Returns statistics about the cleanup.
        """
        stats = {
            "archived_files": 0,
            "deleted_files": 0,
            "bytes_freed": 0,
            "errors": [],
        }

        log_dir = self._store._log_dir
        archive_dir = self._store._archive_dir

        # Process each log type
        for log_type, policy in self._store._policies.items():
            try:
                type_stats = self._cleanup_log_type(log_dir, archive_dir, log_type, policy)
                stats["archived_files"] += type_stats.get("archived", 0)
                stats["deleted_files"] += type_stats.get("deleted", 0)
                stats["bytes_freed"] += type_stats.get("bytes_freed", 0)
            except Exception as e:
                stats["errors"].append(f"{log_type}: {str(e)}")
                logger.error(f"Cleanup error for {log_type}: {e}")

        logger.info(
            f"Cleanup completed: archived={stats['archived_files']}, "
            f"deleted={stats['deleted_files']}, freed={stats['bytes_freed']} bytes"
        )
        return stats

    def _cleanup_log_type(
        self,
        log_dir: Path,
        archive_dir: Path,
        log_type: str,
        policy: RetentionPolicy,
    ) -> dict:
        """Clean up logs for a specific type based on policy."""
        stats = {"archived": 0, "deleted": 0, "bytes_freed": 0}
        cutoff_date = datetime.now() - timedelta(days=policy.retention_days)
        archive_cutoff = datetime.now() - timedelta(days=policy.retention_days * 2)

        # Find log files for this type
        pattern = f"{log_type}_*.ndjson"
        for log_file in log_dir.glob(pattern):
            try:
                # Extract date from filename
                file_date = self._extract_date_from_filename(log_file.name)
                if not file_date:
                    continue

                file_size = log_file.stat().st_size

                # Check if file should be archived
                if file_date < cutoff_date:
                    if policy.archive_before_delete:
                        # Archive the file
                        archived_path = self._archive_file(log_file, archive_dir)
                        if archived_path:
                            stats["archived"] += 1
                            logger.info(f"Archived: {log_file} -> {archived_path}")
                    else:
                        # Delete directly
                        log_file.unlink()
                        stats["deleted"] += 1
                        stats["bytes_freed"] += file_size
                        logger.info(f"Deleted: {log_file}")

            except Exception as e:
                logger.error(f"Error processing {log_file}: {e}")

        # Clean up old archives
        for archive_file in archive_dir.glob(f"{log_type}_*.ndjson.gz"):
            try:
                file_date = self._extract_date_from_filename(archive_file.name.replace(".gz", ""))
                if file_date and file_date < archive_cutoff:
                    file_size = archive_file.stat().st_size
                    archive_file.unlink()
                    stats["deleted"] += 1
                    stats["bytes_freed"] += file_size
                    logger.info(f"Deleted old archive: {archive_file}")
            except Exception as e:
                logger.error(f"Error deleting archive {archive_file}: {e}")

        return stats

    def _extract_date_from_filename(self, filename: str) -> Optional[datetime]:
        """Extract date from log filename."""
        # Expected format: type_YYYY-MM-DD.ndjson or type_YYYYMMDD_HHMMSS.ndjson
        try:
            parts = filename.replace(".ndjson", "").split("_")
            if len(parts) >= 2:
                date_part = parts[1]
                if "-" in date_part:
                    return datetime.strptime(date_part, "%Y-%m-%d")
                elif len(date_part) == 8:
                    return datetime.strptime(date_part, "%Y%m%d")
        except ValueError:
            pass
        return None

    def _archive_file(self, source: Path, archive_dir: Path) -> Optional[Path]:
        """Archive a log file using gzip compression."""
        try:
            archive_path = archive_dir / f"{source.name}.gz"
            with open(source, "rb") as f_in:
                with gzip.open(archive_path, "wb") as f_out:
                    shutil.copyfileobj(f_in, f_out)
            # Delete original after successful archive
            source.unlink()
            return archive_path
        except Exception as e:
            logger.error(f"Failed to archive {source}: {e}")
            return None

    def get_retention_status(self) -> dict:
        """Get current retention status and statistics."""
        log_dir = self._store._log_dir
        archive_dir = self._store._archive_dir

        status = {
            "policies": {},
            "log_files": {},
            "archive_files": {},
            "total_size_bytes": 0,
            "archive_size_bytes": 0,
        }

        # Add policy info
        for log_type, policy in self._store._policies.items():
            status["policies"][log_type] = {
                "retention_days": policy.retention_days,
                "archive_before_delete": policy.archive_before_delete,
                "max_file_size_mb": policy.max_file_size_mb,
            }

        # Count log files
        for log_file in log_dir.glob("*.ndjson"):
            log_type = log_file.name.split("_")[0]
            if log_type not in status["log_files"]:
                status["log_files"][log_type] = {"count": 0, "size_bytes": 0}
            status["log_files"][log_type]["count"] += 1
            file_size = log_file.stat().st_size
            status["log_files"][log_type]["size_bytes"] += file_size
            status["total_size_bytes"] += file_size

        # Count archive files
        for archive_file in archive_dir.glob("*.ndjson.gz"):
            log_type = archive_file.name.split("_")[0]
            if log_type not in status["archive_files"]:
                status["archive_files"][log_type] = {"count": 0, "size_bytes": 0}
            status["archive_files"][log_type]["count"] += 1
            file_size = archive_file.stat().st_size
            status["archive_files"][log_type]["size_bytes"] += file_size
            status["archive_size_bytes"] += file_size

        return status


# Convenience function to create audit log entry
def create_audit_entry(
    log_type: str,
    action: str,
    user_id: str,
    ip_address: str,
    details: Optional[dict] = None,
    request_id: Optional[str] = None,
) -> AuditLogEntry:
    """Create a new audit log entry."""
    return AuditLogEntry(
        timestamp=time.time(),
        log_type=log_type,
        action=action,
        user_id=user_id,
        ip_address=ip_address,
        details=details or {},
        request_id=request_id,
    )


# Global instances (initialized on import or explicitly)
_audit_store: Optional[AuditLogStore] = None
_retention_manager: Optional[AuditLogRetentionManager] = None


def init_audit_system(
    log_dir: str = "/var/log/veloz/audit",
    policies: Optional[dict[str, RetentionPolicy]] = None,
    cleanup_interval_hours: int = 24,
) -> tuple[AuditLogStore, AuditLogRetentionManager]:
    """Initialize the audit logging system."""
    global _audit_store, _retention_manager

    _audit_store = AuditLogStore(log_dir=log_dir, policies=policies)
    _retention_manager = AuditLogRetentionManager(
        log_store=_audit_store,
        cleanup_interval_hours=cleanup_interval_hours,
    )
    _retention_manager.start()

    return _audit_store, _retention_manager


def get_audit_store() -> Optional[AuditLogStore]:
    """Get the global audit store instance."""
    return _audit_store


def get_retention_manager() -> Optional[AuditLogRetentionManager]:
    """Get the global retention manager instance."""
    return _retention_manager


def shutdown_audit_system() -> None:
    """Shutdown the audit logging system."""
    global _audit_store, _retention_manager

    if _retention_manager:
        _retention_manager.stop()
        _retention_manager = None

    if _audit_store:
        _audit_store.close()
        _audit_store = None
