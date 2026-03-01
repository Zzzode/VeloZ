#!/usr/bin/env bash
set -euo pipefail

# Send rollback alert to engineering team
# Usage: ./scripts/alert_rollback.sh "Reason for rollback"

REASON=${1:-"Automatic rollback triggered"}
TIMESTAMP=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

echo "=== Sending Rollback Alert ==="
echo "Timestamp: ${TIMESTAMP}"
echo "Reason: ${REASON}"
echo ""

# Check for alerting tools
if command -v mail >/dev/null 2>&1; then
  # Send email via mail command
  RECIPIENT="${VELOZ_ALERT_EMAIL:-oncall@veloz.example.com}"
  SUBJECT="[ALERT] VeloZ Gateway Rollback - ${TIMESTAMP}"

  echo "Sending email alert to ${RECIPIENT}..."

  mail -s "${SUBJECT}" "${RECIPIENT}" <<EOF
VeloZ Gateway Rollback Alert

Timestamp: ${TIMESTAMP}
Reason: ${REASON}

Action Taken:
- Traffic routed to Python Gateway (port 8080)
- C++ Gateway stopped

Next Steps:
1. Check logs: /var/log/veloz/
2. Investigate root cause
3. Fix identified issues
4. Re-deploy C++ Gateway

Monitoring:
- Health: http://127.0.0.1:8080/health
- Metrics: http://127.0.0.1:8080/metrics

This is an automated alert from VeloZ Gateway Migration.
EOF

  echo "Email alert sent successfully"
fi

# Check for Slack webhook
if [[ -n "${VELOZ_SLACK_WEBHOOK:-}" ]]; then
  echo "Sending Slack alert..."

  curl -s -X POST -H 'Content-type: application/json' \
    --data "{
      \"text\": \"VeloZ Gateway Rollback Alert\",
      \"attachments\": [{
        \"color\": \"danger\",
        \"fields\": [
          {\"title\": \"Timestamp\", \"value\": \"${TIMESTAMP}\", \"short\": true},
          {\"title\": \"Reason\", \"value\": \"${REASON}\", \"short\": false},
          {\"title\": \"Action\", \"value\": \"Traffic routed to Python Gateway\", \"short\": false}
        ]
      }]
    }" \
    "${VELOZ_SLACK_WEBHOOK}" || echo "Warning: Failed to send Slack alert"

  echo "Slack alert sent"
fi

# Check for PagerDuty integration
if [[ -n "${VELOZ_PAGERDUTY_KEY:-}" ]]; then
  echo "Sending PagerDuty alert..."

  curl -s -X POST -H 'Content-type: application/json' \
    -H "Authorization: Token token=${VELOZ_PAGERDUTY_KEY}" \
    --data "{
      \"incident\": {
        \"type\": \"incident\",
        \"title\": \"VeloZ Gateway Rollback\",
        \"service\": {\"id\": \"${VELOZ_PAGERDUTY_SERVICE_ID:-}\", \"type\": \"service_reference\"},
        \"body\": {\"type\": \"incident_body\", \"details\": \"${REASON}\"}
      }
    }" \
    "https://api.pagerduty.com/incidents" || echo "Warning: Failed to send PagerDuty alert"

  echo "PagerDuty alert sent"
fi

# Fallback: Print alert details for manual notification
if ! command -v mail >/dev/null 2>&1 && [[ -z "${VELOZ_SLACK_WEBHOOK:-}" ]] && [[ -z "${VELOZ_PAGERDUTY_KEY:-}" ]]; then
  echo "No alerting tools configured. Manual notification required:"
  echo ""
  echo "=========================================="
  echo "        VELOZ GATEWAY ROLLBACK ALERT"
  echo "=========================================="
  echo ""
  echo "Timestamp: ${TIMESTAMP}"
  echo "Reason: ${REASON}"
  echo ""
  echo "Action Taken:"
  echo "  - Traffic routed to Python Gateway (port 8080)"
  echo "  - C++ Gateway stopped"
  echo ""
  echo "Next Steps:"
  echo "  1. Check logs: /var/log/veloz/"
  echo "  2. Investigate root cause"
  echo "  3. Fix identified issues"
  echo "  4. Re-deploy C++ Gateway"
  echo ""
  echo "Contact:"
  echo "  - On-call: oncall@veloz.example.com"
  echo "  - Engineering: engineering@veloz.example.com"
  echo ""
  echo "=========================================="
fi

echo ""
echo "Alert sent successfully"
