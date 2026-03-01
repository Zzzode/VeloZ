#!/bin/bash
# VeloZ Kubernetes Deployment Script
# Supports both raw manifests and Helm deployments

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HELM_DIR="${SCRIPT_DIR}/../helm/veloz"
NAMESPACE="${VELOZ_NAMESPACE:-veloz}"
ENVIRONMENT="${VELOZ_ENVIRONMENT:-dev}"
USE_HELM="${VELOZ_USE_HELM:-false}"
RELEASE_NAME="veloz"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

function log() {
    echo -e "${GREEN}[$(date '+%Y-%m-%d %H:%M:%S')] ${1}${NC}"
}

function log_warning() {
    echo -e "${YELLOW}[$(date '+%Y-%m-%d %H:%M:%S')] ${1}${NC}"
}

function log_error() {
    echo -e "${RED}[$(date '+%Y-%m-%d %H:%M:%S')] ${1}${NC}"
}

function check_kubectl() {
    if ! command -v kubectl &> /dev/null; then
        log_error "kubectl not found. Please install kubectl and configure your Kubernetes context."
        exit 1
    fi
}

function check_context() {
    if ! kubectl config current-context &> /dev/null; then
        log_error "Kubernetes context not configured. Please run 'kubectl config use-context' to set your context."
        exit 1
    fi

    log "Current context: $(kubectl config current-context)"
}

function create_namespace() {
    log "Creating namespace: ${NAMESPACE}"

    if kubectl get namespace "${NAMESPACE}" &> /dev/null; then
        log_warning "Namespace ${NAMESPACE} already exists"
    else
        kubectl create namespace "${NAMESPACE}"
    fi
}

function apply_config() {
    log "Applying VeloZ configuration"

    kubectl apply -f "${SCRIPT_DIR}/deployment.yaml" -n "${NAMESPACE}"

    log "Configuration applied successfully"
}

function create_secrets() {
    log_warning "You must create the veloz-api-keys secret with your Binance API key and secret"
    log_warning "Run the following command to create the secret:"
    echo ""
    echo "kubectl create secret generic veloz-api-keys \\"
    echo "  --namespace ${NAMESPACE} \\"
    echo "  --from-literal=binance-api-key=\"YOUR_API_KEY\" \\"
    echo "  --from-literal=binance-api-secret=\"YOUR_API_SECRET\""
    echo ""

    read -p "Do you want to create the secret now? (y/N): " -n 1 -r
    echo ""

    if [[ $REPLY =~ ^[Yy]$ ]]; then
        read -p "Enter Binance API Key: " API_KEY
        read -p "Enter Binance API Secret: " API_SECRET

        kubectl create secret generic veloz-api-keys \
            --namespace "${NAMESPACE}" \
            --from-literal=binance-api-key="${API_KEY}" \
            --from-literal=binance-api-secret="${API_SECRET}"

        log "Secret created successfully"
    fi
}

function wait_for_deployment() {
    log "Waiting for deployment to be ready"

    if ! kubectl rollout status deployment/veloz-engine -n "${NAMESPACE}" --timeout=60s; then
        log_error "Deployment failed to become ready"
        kubectl describe deployment/veloz-engine -n "${NAMESPACE}"
        exit 1
    fi

    log "Deployment ready"
}

function check_pods() {
    log "Checking pod status"

    local pod_count=0
    while [ "$pod_count" -lt 1 ]; do
        pod_count=$(kubectl get pods -n "${NAMESPACE}" --field-selector=status.phase=Running -o jsonpath='{.items[*].metadata.name}' | wc -w)
        sleep 2
    done

    log "Running pods: ${pod_count}"

    # Check for any failed containers
    local failed_containers=$(kubectl get pods -n "${NAMESPACE}" -o jsonpath='{.items[*].status.containerStatuses[?(@.state.terminated.reason == "Error")].name}' 2>/dev/null)
    if [ -n "$failed_containers" ]; then
        log_warning "Found failed containers: ${failed_containers}"
        kubectl describe pods -n "${NAMESPACE}"
    fi
}

function port_forward() {
    log_warning "You can forward the API port to your local machine:"
    echo ""
    echo "kubectl port-forward service/veloz-engine 8080:8080 -n ${NAMESPACE}"
    echo ""

    read -p "Do you want to forward the port now? (y/N): " -n 1 -r
    echo ""

    if [[ $REPLY =~ ^[Yy]$ ]]; then
        log "Forwarding port 8080 to local machine"
        kubectl port-forward service/veloz-engine 8080:8080 -n "${NAMESPACE}"
    fi
}

function show_dashboard() {
    log_warning "You can view the Kubernetes dashboard:"
    echo ""
    echo "kubectl proxy"
    echo "Open: http://localhost:8001/api/v1/namespaces/kubernetes-dashboard/services/https:kubernetes-dashboard:/proxy/"
    echo ""
}

function main() {
    log "Starting VeloZ Kubernetes Deployment"

    check_kubectl
    check_context
    create_namespace
    apply_config
    create_secrets
    wait_for_deployment
    check_pods

    log "VeloZ Engine deployed successfully!"

    port_forward
    show_dashboard
}

# Handle cleanup on interruption
function cleanup() {
    log_warning "Interrupted. Cleaning up..."
    exit 1
}

# Main execution
trap cleanup INT TERM

function deploy_helm() {
    log "Deploying VeloZ using Helm to ${ENVIRONMENT} environment"

    if ! command -v helm &> /dev/null; then
        log_error "helm not found. Please install Helm 3."
        exit 1
    fi

    # Determine values file
    local values_file=""
    case "$ENVIRONMENT" in
        staging)
            values_file="-f ${HELM_DIR}/values-staging.yaml"
            ;;
        production)
            values_file="-f ${HELM_DIR}/values-production.yaml"
            ;;
    esac

    # Update dependencies
    log "Updating Helm dependencies..."
    helm dependency update "$HELM_DIR" 2>/dev/null || true

    # Check if release exists
    if helm status "$RELEASE_NAME" -n "$NAMESPACE" &>/dev/null; then
        log "Upgrading existing Helm release..."
        # shellcheck disable=SC2086
        helm upgrade "$RELEASE_NAME" "$HELM_DIR" \
            --namespace "$NAMESPACE" \
            $values_file \
            --wait \
            --timeout 5m
    else
        log "Installing new Helm release..."
        # shellcheck disable=SC2086
        helm install "$RELEASE_NAME" "$HELM_DIR" \
            --namespace "$NAMESPACE" \
            --create-namespace \
            $values_file \
            --wait \
            --timeout 5m
    fi

    log "Helm deployment complete!"
}

if [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -h, --help       Show this help message and exit"
    echo "  --helm           Use Helm for deployment"
    echo "  --env ENV        Target environment: dev, staging, production"
    echo "  --namespace NS   Kubernetes namespace"
    echo ""
    echo "Environment Variables:"
    echo "  VELOZ_USE_HELM       Use Helm for deployment (true/false)"
    echo "  VELOZ_ENVIRONMENT    Target environment"
    echo "  VELOZ_NAMESPACE      Kubernetes namespace"
    echo ""
    echo "Examples:"
    echo "  $0                           # Deploy using manifests"
    echo "  $0 --helm                    # Deploy using Helm"
    echo "  $0 --helm --env production   # Deploy to production with Helm"
    echo ""
    exit 0
fi

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --helm)
            USE_HELM="true"
            shift
            ;;
        --env)
            ENVIRONMENT="$2"
            shift 2
            ;;
        --namespace)
            NAMESPACE="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

if [ "$USE_HELM" == "true" ]; then
    check_kubectl
    check_context
    deploy_helm
else
    main "$@"
fi
