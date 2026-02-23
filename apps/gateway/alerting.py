#!/usr/bin/env python3
"""
VeloZ Gateway Alerting Module

Provides programmatic alerting capabilities for the Python gateway.
Supports direct integration with PagerDuty, Opsgenie, and Slack.
"""

import json
import os
import time
import threading
import urllib.request
import urllib.error
from typing import Optional, Dict, Any, List
from dataclasses import dataclass, field, asdict
from enum import Enum
from datetime import datetime, timezone


# =============================================================================
# Alert Types and Severity
# =============================================================================

class AlertSeverity(Enum):
    """Alert severity levels."""
    CRITICAL = "critical"
    WARNING = "warning"
    INFO = "info"


class AlertStatus(Enum):
    """Alert status."""
    FIRING = "firing"
    RESOLVED = "resolved"


@dataclass
class Alert:
    """Represents an alert."""
    name: str
    severity: AlertSeverity
    summary: str
    description: str
    service: str = "veloz-gateway"
    status: AlertStatus = AlertStatus.FIRING
    labels: Dict[str, str] = field(default_factory=dict)
    annotations: Dict[str, str] = field(default_factory=dict)
    timestamp: str = field(default_factory=lambda: datetime.now(timezone.utc).isoformat())
    fingerprint: Optional[str] = None

    def __post_init__(self):
        if not self.fingerprint:
            # Generate fingerprint from name and labels
            label_str = "|".join(f"{k}={v}" for k, v in sorted(self.labels.items()))
            self.fingerprint = f"{self.name}|{label_str}"


# =============================================================================
# PagerDuty Integration
# =============================================================================

class PagerDutyClient:
    """Client for PagerDuty Events API v2."""

    API_URL = "https://events.pagerduty.com/v2/enqueue"

    def __init__(self, routing_key: Optional[str] = None):
        self.routing_key = routing_key or os.environ.get("PAGERDUTY_SERVICE_KEY", "")
        self.enabled = bool(self.routing_key)

    def send_alert(self, alert: Alert) -> bool:
        """Send an alert to PagerDuty."""
        if not self.enabled:
            return False

        # Map severity to PagerDuty severity
        severity_map = {
            AlertSeverity.CRITICAL: "critical",
            AlertSeverity.WARNING: "warning",
            AlertSeverity.INFO: "info",
        }

        # Map status to event action
        action = "trigger" if alert.status == AlertStatus.FIRING else "resolve"

        payload = {
            "routing_key": self.routing_key,
            "event_action": action,
            "dedup_key": alert.fingerprint,
            "payload": {
                "summary": alert.summary,
                "severity": severity_map.get(alert.severity, "warning"),
                "source": alert.service,
                "component": alert.labels.get("component", "unknown"),
                "group": alert.labels.get("service", "veloz"),
                "class": alert.name,
                "custom_details": {
                    "description": alert.description,
                    "timestamp": alert.timestamp,
                    **alert.labels,
                    **alert.annotations,
                },
            },
        }

        return self._send_request(payload)

    def _send_request(self, payload: Dict[str, Any]) -> bool:
        """Send request to PagerDuty API."""
        try:
            data = json.dumps(payload).encode("utf-8")
            req = urllib.request.Request(
                self.API_URL,
                data=data,
                headers={"Content-Type": "application/json"},
                method="POST",
            )
            with urllib.request.urlopen(req, timeout=10) as resp:
                return resp.status == 202
        except Exception as e:
            print(f"PagerDuty API error: {e}")
            return False


# =============================================================================
# Opsgenie Integration
# =============================================================================

class OpsgenieClient:
    """Client for Opsgenie Alerts API."""

    API_URL = "https://api.opsgenie.com/v2/alerts"

    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key or os.environ.get("OPSGENIE_API_KEY", "")
        self.enabled = bool(self.api_key)

    def send_alert(self, alert: Alert) -> bool:
        """Send an alert to Opsgenie."""
        if not self.enabled:
            return False

        if alert.status == AlertStatus.FIRING:
            return self._create_alert(alert)
        else:
            return self._close_alert(alert)

    def _create_alert(self, alert: Alert) -> bool:
        """Create a new alert in Opsgenie."""
        # Map severity to priority
        priority_map = {
            AlertSeverity.CRITICAL: "P1",
            AlertSeverity.WARNING: "P3",
            AlertSeverity.INFO: "P5",
        }

        payload = {
            "message": alert.summary,
            "alias": alert.fingerprint,
            "description": alert.description,
            "priority": priority_map.get(alert.severity, "P3"),
            "source": alert.service,
            "tags": [
                f"severity:{alert.severity.value}",
                f"service:{alert.labels.get('service', 'veloz')}",
                "veloz",
            ],
            "details": {
                "alert_name": alert.name,
                "timestamp": alert.timestamp,
                **alert.labels,
                **alert.annotations,
            },
        }

        return self._send_request(self.API_URL, payload, "POST")

    def _close_alert(self, alert: Alert) -> bool:
        """Close an alert in Opsgenie."""
        url = f"{self.API_URL}/{alert.fingerprint}/close?identifierType=alias"
        payload = {
            "source": alert.service,
            "note": f"Alert resolved: {alert.summary}",
        }
        return self._send_request(url, payload, "POST")

    def _send_request(self, url: str, payload: Dict[str, Any], method: str) -> bool:
        """Send request to Opsgenie API."""
        try:
            data = json.dumps(payload).encode("utf-8")
            req = urllib.request.Request(
                url,
                data=data,
                headers={
                    "Content-Type": "application/json",
                    "Authorization": f"GenieKey {self.api_key}",
                },
                method=method,
            )
            with urllib.request.urlopen(req, timeout=10) as resp:
                return resp.status in (200, 202)
        except Exception as e:
            print(f"Opsgenie API error: {e}")
            return False


# =============================================================================
# Slack Integration
# =============================================================================

class SlackClient:
    """Client for Slack Webhooks."""

    def __init__(self, webhook_url: Optional[str] = None):
        self.webhook_url = webhook_url or os.environ.get("SLACK_WEBHOOK_URL", "")
        self.enabled = bool(self.webhook_url)

    def send_alert(self, alert: Alert, channel: Optional[str] = None) -> bool:
        """Send an alert to Slack."""
        if not self.enabled:
            return False

        # Color based on severity and status
        if alert.status == AlertStatus.RESOLVED:
            color = "good"
        elif alert.severity == AlertSeverity.CRITICAL:
            color = "danger"
        elif alert.severity == AlertSeverity.WARNING:
            color = "warning"
        else:
            color = "#439FE0"

        status_emoji = ":white_check_mark:" if alert.status == AlertStatus.RESOLVED else ":rotating_light:"

        payload = {
            "attachments": [
                {
                    "color": color,
                    "title": f"{status_emoji} {alert.name}",
                    "text": alert.summary,
                    "fields": [
                        {
                            "title": "Severity",
                            "value": alert.severity.value.upper(),
                            "short": True,
                        },
                        {
                            "title": "Status",
                            "value": alert.status.value.upper(),
                            "short": True,
                        },
                        {
                            "title": "Service",
                            "value": alert.labels.get("service", alert.service),
                            "short": True,
                        },
                        {
                            "title": "Component",
                            "value": alert.labels.get("component", "unknown"),
                            "short": True,
                        },
                    ],
                    "footer": "VeloZ Alerting",
                    "ts": int(time.time()),
                }
            ],
        }

        if channel:
            payload["channel"] = channel

        if alert.description:
            payload["attachments"][0]["fields"].append({
                "title": "Description",
                "value": alert.description,
                "short": False,
            })

        return self._send_request(payload)

    def _send_request(self, payload: Dict[str, Any]) -> bool:
        """Send request to Slack webhook."""
        try:
            data = json.dumps(payload).encode("utf-8")
            req = urllib.request.Request(
                self.webhook_url,
                data=data,
                headers={"Content-Type": "application/json"},
                method="POST",
            )
            with urllib.request.urlopen(req, timeout=10) as resp:
                return resp.status == 200
        except Exception as e:
            print(f"Slack webhook error: {e}")
            return False


# =============================================================================
# Alert Manager
# =============================================================================

class AlertManager:
    """
    Manages alert routing and deduplication.
    Coordinates sending alerts to multiple destinations.
    """

    def __init__(
        self,
        pagerduty_key: Optional[str] = None,
        opsgenie_key: Optional[str] = None,
        slack_webhook: Optional[str] = None,
    ):
        self.pagerduty = PagerDutyClient(pagerduty_key)
        self.opsgenie = OpsgenieClient(opsgenie_key)
        self.slack = SlackClient(slack_webhook)

        # Track active alerts for deduplication
        self._active_alerts: Dict[str, Alert] = {}
        self._lock = threading.Lock()

    def fire_alert(
        self,
        name: str,
        severity: AlertSeverity,
        summary: str,
        description: str,
        service: str = "veloz-gateway",
        labels: Optional[Dict[str, str]] = None,
        annotations: Optional[Dict[str, str]] = None,
    ) -> Alert:
        """Fire a new alert or update an existing one."""
        alert = Alert(
            name=name,
            severity=severity,
            summary=summary,
            description=description,
            service=service,
            status=AlertStatus.FIRING,
            labels=labels or {},
            annotations=annotations or {},
        )

        with self._lock:
            # Check if alert is already active
            if alert.fingerprint in self._active_alerts:
                # Update existing alert
                self._active_alerts[alert.fingerprint] = alert
                return alert

            # New alert
            self._active_alerts[alert.fingerprint] = alert

        # Send to all configured destinations
        self._send_alert(alert)
        return alert

    def resolve_alert(self, fingerprint: str) -> Optional[Alert]:
        """Resolve an active alert."""
        with self._lock:
            if fingerprint not in self._active_alerts:
                return None

            alert = self._active_alerts.pop(fingerprint)
            alert.status = AlertStatus.RESOLVED
            alert.timestamp = datetime.now(timezone.utc).isoformat()

        # Send resolution to all configured destinations
        self._send_alert(alert)
        return alert

    def resolve_by_name(self, name: str, labels: Optional[Dict[str, str]] = None) -> List[Alert]:
        """Resolve all alerts matching name and optional labels."""
        resolved = []
        labels = labels or {}

        with self._lock:
            to_resolve = []
            for fp, alert in self._active_alerts.items():
                if alert.name == name:
                    # Check if labels match
                    if all(alert.labels.get(k) == v for k, v in labels.items()):
                        to_resolve.append(fp)

            for fp in to_resolve:
                alert = self._active_alerts.pop(fp)
                alert.status = AlertStatus.RESOLVED
                alert.timestamp = datetime.now(timezone.utc).isoformat()
                resolved.append(alert)

        # Send resolutions
        for alert in resolved:
            self._send_alert(alert)

        return resolved

    def _send_alert(self, alert: Alert):
        """Send alert to all configured destinations."""
        # Route based on severity
        if alert.severity == AlertSeverity.CRITICAL:
            self.pagerduty.send_alert(alert)
            self.opsgenie.send_alert(alert)
            self.slack.send_alert(alert)
        elif alert.severity == AlertSeverity.WARNING:
            self.opsgenie.send_alert(alert)
            self.slack.send_alert(alert)
        else:
            self.slack.send_alert(alert)

    def get_active_alerts(self) -> List[Alert]:
        """Get all active alerts."""
        with self._lock:
            return list(self._active_alerts.values())

    def get_alert_count(self) -> Dict[str, int]:
        """Get count of active alerts by severity."""
        with self._lock:
            counts = {s.value: 0 for s in AlertSeverity}
            for alert in self._active_alerts.values():
                counts[alert.severity.value] += 1
            return counts


# =============================================================================
# Global Instance
# =============================================================================

_alert_manager: Optional[AlertManager] = None


def get_alert_manager() -> AlertManager:
    """Get or create the global alert manager."""
    global _alert_manager
    if _alert_manager is None:
        _alert_manager = AlertManager()
    return _alert_manager


def init_alerting(
    pagerduty_key: Optional[str] = None,
    opsgenie_key: Optional[str] = None,
    slack_webhook: Optional[str] = None,
) -> AlertManager:
    """Initialize the alerting system."""
    global _alert_manager
    _alert_manager = AlertManager(pagerduty_key, opsgenie_key, slack_webhook)
    return _alert_manager


# =============================================================================
# Convenience Functions
# =============================================================================

def fire_critical(
    name: str,
    summary: str,
    description: str,
    service: str = "veloz-gateway",
    **labels,
) -> Alert:
    """Fire a critical alert."""
    return get_alert_manager().fire_alert(
        name=name,
        severity=AlertSeverity.CRITICAL,
        summary=summary,
        description=description,
        service=service,
        labels=labels,
    )


def fire_warning(
    name: str,
    summary: str,
    description: str,
    service: str = "veloz-gateway",
    **labels,
) -> Alert:
    """Fire a warning alert."""
    return get_alert_manager().fire_alert(
        name=name,
        severity=AlertSeverity.WARNING,
        summary=summary,
        description=description,
        service=service,
        labels=labels,
    )


def fire_info(
    name: str,
    summary: str,
    description: str,
    service: str = "veloz-gateway",
    **labels,
) -> Alert:
    """Fire an info alert."""
    return get_alert_manager().fire_alert(
        name=name,
        severity=AlertSeverity.INFO,
        summary=summary,
        description=description,
        service=service,
        labels=labels,
    )


def resolve(fingerprint: str) -> Optional[Alert]:
    """Resolve an alert by fingerprint."""
    return get_alert_manager().resolve_alert(fingerprint)
