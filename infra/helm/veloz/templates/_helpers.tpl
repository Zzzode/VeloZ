{{/*
Expand the name of the chart.
*/}}
{{- define "veloz.name" -}}
{{- default .Chart.Name .Values.nameOverride | trunc 63 | trimSuffix "-" }}
{{- end }}

{{/*
Create a default fully qualified app name.
*/}}
{{- define "veloz.fullname" -}}
{{- if .Values.fullnameOverride }}
{{- .Values.fullnameOverride | trunc 63 | trimSuffix "-" }}
{{- else }}
{{- $name := default .Chart.Name .Values.nameOverride }}
{{- if contains $name .Release.Name }}
{{- .Release.Name | trunc 63 | trimSuffix "-" }}
{{- else }}
{{- printf "%s-%s" .Release.Name $name | trunc 63 | trimSuffix "-" }}
{{- end }}
{{- end }}
{{- end }}

{{/*
Create chart name and version as used by the chart label.
*/}}
{{- define "veloz.chart" -}}
{{- printf "%s-%s" .Chart.Name .Chart.Version | replace "+" "_" | trunc 63 | trimSuffix "-" }}
{{- end }}

{{/*
Common labels
*/}}
{{- define "veloz.labels" -}}
helm.sh/chart: {{ include "veloz.chart" . }}
{{ include "veloz.selectorLabels" . }}
{{- if .Chart.AppVersion }}
app.kubernetes.io/version: {{ .Chart.AppVersion | quote }}
{{- end }}
app.kubernetes.io/managed-by: {{ .Release.Service }}
{{- end }}

{{/*
Selector labels
*/}}
{{- define "veloz.selectorLabels" -}}
app.kubernetes.io/name: {{ include "veloz.name" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end }}

{{/*
Create the name of the service account to use
*/}}
{{- define "veloz.serviceAccountName" -}}
{{- if .Values.serviceAccount.create }}
{{- default (include "veloz.fullname" .) .Values.serviceAccount.name }}
{{- else }}
{{- default "default" .Values.serviceAccount.name }}
{{- end }}
{{- end }}

{{/*
Create the database URL
*/}}
{{- define "veloz.databaseUrl" -}}
{{- if .Values.database.external }}
{{- .Values.database.url }}
{{- else if .Values.database.postgresql.enabled }}
{{- printf "postgresql://%s:$(DB_PASSWORD)@%s-postgresql:5432/%s" .Values.database.postgresql.auth.username (include "veloz.fullname" .) .Values.database.postgresql.auth.database }}
{{- else }}
{{- "sqlite:///veloz_gateway.db" }}
{{- end }}
{{- end }}

{{/*
Environment variables for the engine
*/}}
{{- define "veloz.envVars" -}}
{{- range $key, $value := .Values.engine.env }}
- name: {{ $key }}
  value: {{ $value | quote }}
{{- end }}
{{- end }}

{{/*
Secret environment variables
*/}}
{{- define "veloz.secretEnvVars" -}}
{{- if not .Values.secrets.useExternal }}
- name: VELOZ_BINANCE_API_KEY
  valueFrom:
    secretKeyRef:
      name: {{ include "veloz.fullname" . }}-api-keys
      key: binance-api-key
- name: VELOZ_BINANCE_API_SECRET
  valueFrom:
    secretKeyRef:
      name: {{ include "veloz.fullname" . }}-api-keys
      key: binance-api-secret
{{- end }}
{{- end }}
