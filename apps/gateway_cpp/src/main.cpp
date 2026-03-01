/**
 * @file main.cpp
 * @brief Main entry point for VeloZ C++ Gateway
 *
 * This file initializes and starts the HTTP gateway server with complete
 * component lifecycle management.
 *
 * Initialization order (dependencies first):
 * 1. Metrics Registry (no dependencies)
 * 2. Audit Logger (no dependencies)
 * 3. Authentication (JWT, API Keys, RBAC)
 * 4. Middleware Chain (Auth, Rate Limit, CORS, Audit)
 * 5. Engine Bridge (depends on metrics)
 * 6. Event Broadcaster (depends on engine bridge)
 * 7. Handlers (depend on all above)
 * 8. HTTP Server (depends on all above)
 *
 * Shutdown sequence (reverse order):
 * 1. Stop HTTP server (stop accepting new connections)
 * 2. Close SSE connections (wait for completion)
 * 3. Stop Engine Bridge (wait for event processing)
 * 4. Flush Audit Logs (ensure all logs written)
 * 5. Cleanup Middleware resources
 * 6. Close Metrics export
 *
 * Configuration via environment variables:
 * - VELOZ_HOST (default: 0.0.0.0)
 * - VELOZ_PORT (default: 8080)
 * - VELOZ_AUTH_ENABLED (default: true)
 * - VELOZ_JWT_SECRET (required in production)
 * - VELOZ_JWT_ACCESS_EXPIRY (default: 3600)
 * - VELOZ_JWT_REFRESH_EXPIRY (default: 604800)
 * - VELOZ_RATE_LIMIT_CAPACITY (default: 100)
 * - VELOZ_RATE_LIMIT_REFILL (default: 10.0)
 * - VELOZ_CORS_ORIGIN (default: *)
 * - VELOZ_STATIC_DIR (default: ./apps/ui)
 * - VELOZ_AUDIT_LOG_DIR (default: /var/log/veloz/audit)
 * - VELOZ_ADMIN_PASSWORD (required for admin login)
 */

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/compat/http.h>
#include <kj/debug.h>
#include <kj/filesystem.h>
#include <kj/function.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/vector.h>

// VeloZ core
#include <veloz/core/metrics.h>

// Gateway components
#include "audit/audit_logger.h"
#include "auth/api_key_manager.h"
#include "auth/auth_manager.h"
#include "auth/jwt_manager.h"
#include "auth/rbac.h"
#include "bridge/engine_bridge.h"
#include "bridge/event_broadcaster.h"
#include "gateway_server.h"
#include "handlers/account_handler.h"
#include "handlers/audit_handler.h"
#include "handlers/auth_handler.h"
#include "handlers/config_handler.h"
#include "handlers/health_handler.h"
#include "handlers/market_handler.h"
#include "handlers/metrics_handler.h"
#include "handlers/order_handler.h"
#include "handlers/sse_handler.h"
#include "handlers/static_handler.h"
#include "middleware/audit_middleware.h"
#include "middleware/auth_middleware.h"
#include "middleware/cors_middleware.h"
#include "middleware/metrics_middleware.h"
#include "middleware/rate_limiter.h"
#include "request_context.h"
#include "router.h"
#include "static/static_file_server.h"

namespace veloz {

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Gateway configuration loaded from environment variables
 *
 * All settings can be overridden via environment variables.
 * Defaults are suitable for development only.
 */
struct GatewayConfig {
  // Server settings
  kj::String host = kj::str("0.0.0.0");
  uint16_t port = 8080;

  // Authentication
  bool auth_enabled = true;
  kj::String jwt_secret = kj::str("veloz-default-secret-change-in-production");
  uint32_t jwt_access_expiry_seconds = 3600;    // 1 hour
  uint32_t jwt_refresh_expiry_seconds = 604800; // 7 days
  kj::String admin_password;

  // Rate limiting
  uint32_t rate_limit_capacity = 100;
  double rate_limit_refill_rate = 10.0; // tokens per second
  bool rate_limit_per_user = true;

  // CORS
  kj::String cors_allowed_origin = kj::str("*");
  bool cors_allow_credentials = false;
  int32_t cors_max_age = 86400; // 24 hours

  // Static files
  kj::String static_dir = kj::str("./apps/ui");
  bool static_cache_enabled = true;
  uint32_t static_cache_max_age = 3600; // 1 hour

  // Audit logging
  kj::String audit_log_dir = kj::str("/var/log/veloz/audit");
  bool audit_console_output = false;

  // Engine bridge
  kj::String engine_preset = kj::str("dev");
  size_t event_queue_capacity = 10000;
  uint32_t max_event_subscriptions = 1000;

  // SSE
  uint64_t sse_keepalive_interval_ms = 10000;
  uint64_t sse_retry_ms = 3000;
  size_t sse_max_concurrent_streams = 1000;

  /**
   * @brief Load configuration from environment variables
   *
   * Reads all VELOZ_* environment variables and applies them to config.
   * Uses defaults for any unset variables.
   *
   * Supports both VELOZ_HOST and VELOZ_GATEWAY_HOST for backwards compatibility.
   */
  static GatewayConfig loadFromEnv() {
    GatewayConfig config;

    // Server settings (support both VELOZ_HOST and VELOZ_GATEWAY_HOST)
    if (const char* host_env = std::getenv("VELOZ_HOST")) {
      config.host = kj::heapString(host_env);
    } else if (const char* host_env = std::getenv("VELOZ_GATEWAY_HOST")) {
      config.host = kj::heapString(host_env);
    }
    if (const char* port_env = std::getenv("VELOZ_PORT")) {
      config.port = static_cast<uint16_t>(std::stoi(port_env));
    } else if (const char* port_env = std::getenv("VELOZ_GATEWAY_PORT")) {
      config.port = static_cast<uint16_t>(std::stoi(port_env));
    }

    // Authentication
    if (const char* auth_env = std::getenv("VELOZ_AUTH_ENABLED")) {
      config.auth_enabled = (kj::StringPtr(auth_env) == "true"_kj);
    }
    if (const char* secret_env = std::getenv("VELOZ_JWT_SECRET")) {
      config.jwt_secret = kj::heapString(secret_env);
    }
    if (const char* expiry_env = std::getenv("VELOZ_JWT_ACCESS_EXPIRY")) {
      config.jwt_access_expiry_seconds = static_cast<uint32_t>(std::stoul(expiry_env));
    }
    if (const char* refresh_env = std::getenv("VELOZ_JWT_REFRESH_EXPIRY")) {
      config.jwt_refresh_expiry_seconds = static_cast<uint32_t>(std::stoul(refresh_env));
    }
    if (const char* admin_env = std::getenv("VELOZ_ADMIN_PASSWORD")) {
      config.admin_password = kj::heapString(admin_env);
    }

    // Rate limiting
    if (const char* capacity_env = std::getenv("VELOZ_RATE_LIMIT_CAPACITY")) {
      config.rate_limit_capacity = static_cast<uint32_t>(std::stoul(capacity_env));
    }
    if (const char* refill_env = std::getenv("VELOZ_RATE_LIMIT_REFILL")) {
      config.rate_limit_refill_rate = std::stod(refill_env);
    }
    if (const char* per_user_env = std::getenv("VELOZ_RATE_LIMIT_PER_USER")) {
      config.rate_limit_per_user = (kj::StringPtr(per_user_env) == "true"_kj);
    }

    // CORS
    if (const char* origin_env = std::getenv("VELOZ_CORS_ORIGIN")) {
      config.cors_allowed_origin = kj::heapString(origin_env);
    }
    if (const char* creds_env = std::getenv("VELOZ_CORS_CREDENTIALS")) {
      config.cors_allow_credentials = (kj::StringPtr(creds_env) == "true"_kj);
    }
    if (const char* age_env = std::getenv("VELOZ_CORS_MAX_AGE")) {
      config.cors_max_age = std::stoi(age_env);
    }

    // Static files
    if (const char* static_env = std::getenv("VELOZ_STATIC_DIR")) {
      config.static_dir = kj::heapString(static_env);
    }
    if (const char* cache_env = std::getenv("VELOZ_STATIC_CACHE_ENABLED")) {
      config.static_cache_enabled = (kj::StringPtr(cache_env) == "true"_kj);
    }

    // Audit logging
    if (const char* audit_env = std::getenv("VELOZ_AUDIT_LOG_DIR")) {
      config.audit_log_dir = kj::heapString(audit_env);
    }
    if (const char* console_env = std::getenv("VELOZ_AUDIT_CONSOLE")) {
      config.audit_console_output = (kj::StringPtr(console_env) == "true"_kj);
    }

    // Engine
    if (const char* preset_env = std::getenv("VELOZ_ENGINE_PRESET")) {
      config.engine_preset = kj::heapString(preset_env);
    }

    return config;
  }

  /**
   * @brief Validate configuration and log warnings
   *
   * Logs warnings for potentially insecure configurations.
   * Does not throw - continues with warnings if config is invalid.
   *
   * @throws kj::Exception if configuration is invalid and cannot continue
   */
  void validate() const {
    // Security warnings
    if (jwt_secret == "veloz-default-secret-change-in-production"_kj) {
      KJ_LOG(WARNING, "Using default JWT secret. Set VELOZ_JWT_SECRET in production!");
    }
    if (auth_enabled && jwt_secret.size() < 32) {
      KJ_LOG(WARNING, "JWT secret should be at least 32 characters for security", "current_length",
             jwt_secret.size());
    }
    if (admin_password.size() == 0) {
      KJ_LOG(WARNING, "Admin password not set. Set VELOZ_ADMIN_PASSWORD to enable admin login.");
    }

    // Validation errors (cannot continue)
    if (port == 0) {
      KJ_FAIL_REQUIRE("Invalid port number: 0");
    }
    if (rate_limit_capacity == 0) {
      KJ_FAIL_REQUIRE("Rate limit capacity must be > 0");
    }
    if (rate_limit_refill_rate <= 0) {
      KJ_FAIL_REQUIRE("Rate limit refill rate must be > 0");
    }
  }
};

// ============================================================================
// Signal Handling
// ============================================================================

namespace {
// Global flag for graceful shutdown
// Uses std::atomic for lock-free access from signal handler
std::atomic<bool> g_shutdown_requested{false};
std::atomic<int> g_shutdown_signal{0};

/**
 * @brief Signal handler for SIGTERM and SIGINT
 *
 * Sets the global shutdown flag when termination signal is received.
 * This handler must be async-signal-safe (only uses atomic operations).
 */
void signal_handler(int signal) {
  // Only use async-signal-safe operations here
  g_shutdown_signal.store(signal, std::memory_order_release);
  g_shutdown_requested.store(true, std::memory_order_release);
}
} // namespace

// ============================================================================
// Component Lifecycle Manager
// ============================================================================

/**
 * @brief Manages component lifecycle and initialization order
 *
 * Ensures components are initialized and destroyed in the correct order.
 * Owns all gateway components.
 */
class GatewayLifecycle {
public:
  explicit GatewayLifecycle(const GatewayConfig& config, kj::AsyncIoContext& io)
      : config_(config), io_(io) {}

  ~GatewayLifecycle() {
    cleanup();
  }

  // Non-copyable, non-movable
  GatewayLifecycle(const GatewayLifecycle&) = delete;
  GatewayLifecycle& operator=(const GatewayLifecycle&) = delete;
  GatewayLifecycle(GatewayLifecycle&&) = delete;
  GatewayLifecycle& operator=(GatewayLifecycle&&) = delete;

  /**
   * @brief Initialize all components in dependency order
   *
   * Order:
   * 1. Metrics Registry
   * 2. Audit Logger
   * 3. Authentication (JWT, API Keys, RBAC)
   * 4. Middleware Chain
   * 5. Engine Bridge
   * 6. Event Broadcaster
   * 7. Handlers
   * 8. Router
   */
  kj::Promise<void> initialize() {
    KJ_LOG(INFO, "Initializing gateway components in dependency order");

    // Step 1: Metrics Registry (no dependencies)
    KJ_LOG(INFO, "[1/8] Initializing Metrics Registry");
    initializeMetrics();

    // Step 2: Audit Logger (no dependencies)
    KJ_LOG(INFO, "[2/8] Initializing Audit Logger");
    initializeAuditLogger();

    // Step 3: Authentication (no dependencies)
    KJ_LOG(INFO, "[3/8] Initializing Authentication");
    initializeAuthentication();

    // Step 4: Middleware Chain (depends on auth)
    KJ_LOG(INFO, "[4/8] Initializing Middleware Chain");
    initializeMiddleware();

    // Step 5: Engine Bridge (depends on metrics)
    KJ_LOG(INFO, "[5/8] Initializing Engine Bridge");
    co_await initializeEngineBridge();

    // Step 6: Event Broadcaster (no dependencies)
    KJ_LOG(INFO, "[6/8] Initializing Event Broadcaster");
    initializeEventBroadcaster();

    // Step 7: Handlers (depend on all above)
    KJ_LOG(INFO, "[7/8] Initializing Request Handlers");
    initializeHandlers();

    // Step 8: Router (depends on handlers)
    KJ_LOG(INFO, "[8/8] Initializing Router and Registering Routes");
    initializeRouter();

    KJ_LOG(INFO, "All components initialized successfully");
  }

  /**
   * @brief Run the HTTP server
   *
   * Starts listening for connections and handles requests.
   */
  kj::Promise<void> run() {
    KJ_LOG(INFO, "Starting HTTP server", "host", config_.host, "port", config_.port);

    // Bind to the configured address
    auto& network = io_.provider->getNetwork();
    auto addr = network.parseAddress(kj::str(config_.host, ":", config_.port)).wait(io_.waitScope);
    auto listener = addr->listen();

    KJ_LOG(INFO, "HTTP server listening", "address", kj::str(config_.host, ":", config_.port));

    // Store the listener for cleanup
    listener_ = kj::mv(listener);

    // Create GatewayHttpService that integrates Router with kj::HttpServer
    auto gateway_service = kj::heap<gateway::GatewayServer>(*header_table_, *router_);

    // Create HTTP server with our service
    auto http_server =
        kj::heap<kj::HttpServer>(io_.provider->getTimer(), *header_table_, *gateway_service);

    KJ_LOG(INFO, "Gateway HTTP server started successfully");

    // Start listening for connections
    // The HTTP server will dispatch requests to our GatewayService
    // which routes them through the Router to handlers
    KJ_IF_SOME(ref, listener_) {
      auto listenPromise = http_server->listenHttp(*ref);

      // Wait for shutdown signal
      co_await waitForShutdown();

      // Cancel the listen promise (stop accepting new connections)
      // The promise will be detached, so we just wait for shutdown
    }

    KJ_LOG(INFO, "HTTP server stopped accepting new connections");
  }

  /**
   * @brief Wait for shutdown signal
   */
  kj::Promise<void> waitForShutdown() {
    while (!g_shutdown_requested.load(std::memory_order_acquire)) {
      co_await io_.provider->getTimer().afterDelay(100 * kj::MILLISECONDS);
    }
    KJ_LOG(INFO, "Shutdown signal received");
  }

  /**
   * @brief Graceful shutdown sequence
   *
   * Order:
   * 1. Stop HTTP server
   * 2. Close SSE connections
   * 3. Stop Engine Bridge
   * 4. Flush Audit Logs
   * 5. Cleanup Middleware
   */
  void cleanup() {
    if (cleaned_up_.exchange(true)) {
      return; // Already cleaned up
    }

    KJ_LOG(INFO, "Starting graceful shutdown sequence");

    // Step 1: Stop HTTP server
    KJ_LOG(INFO, "[1/5] Stopping HTTP server");
    stopHttpServer();

    // Step 2: Close SSE connections
    KJ_LOG(INFO, "[2/5] Closing SSE connections");
    closeSseConnections();

    // Step 3: Stop Engine Bridge
    KJ_LOG(INFO, "[3/5] Stopping Engine Bridge");
    stopEngineBridge();

    // Step 4: Flush Audit Logs
    KJ_LOG(INFO, "[4/5] Flushing Audit Logs");
    flushAuditLogs();

    // Step 5: Cleanup Middleware
    KJ_LOG(INFO, "[5/5] Cleaning up Middleware");
    cleanupMiddleware();

    KJ_LOG(INFO, "Graceful shutdown complete");
  }

private:
  // ===========================================================================
  // Initialization Methods
  // ===========================================================================

  void initializeMetrics() {
    // Use the global metrics registry from veloz::core
    metrics_registry_ = &core::global_metrics();

    // Register gateway metrics
    metrics_registry_->register_counter("gateway_requests_total", "Total number of HTTP requests");
    metrics_registry_->register_histogram("gateway_request_duration_seconds",
                                          "HTTP request duration in seconds");
    metrics_registry_->register_gauge("gateway_active_connections",
                                      "Number of active HTTP connections");
    metrics_registry_->register_counter("gateway_errors_total", "Total number of errors");

    KJ_LOG(INFO, "Metrics Registry initialized");
  }

  void initializeAuditLogger() {
    // Configure audit logger
    gateway::audit::AuditLoggerConfig audit_config;
    audit_config.log_dir = config_.audit_log_dir;
    audit_config.enable_console_output = config_.audit_console_output;
    audit_config.queue_capacity = 10000;
    audit_config.max_file_size = 100 * 1024 * 1024; // 100MB
    audit_config.retention_days = 30;

    audit_logger_ = kj::heap<gateway::audit::AuditLogger>(kj::mv(audit_config));

    KJ_LOG(INFO, "Audit Logger initialized", "log_dir", config_.audit_log_dir);
  }

  void initializeAuthentication() {
    // Initialize JWT manager
    auto jwt_mgr = kj::heap<gateway::auth::JwtManager>(
        config_.jwt_secret,
        kj::none, // Use same secret for refresh tokens
        config_.jwt_access_expiry_seconds, config_.jwt_refresh_expiry_seconds);
    jwt_manager_ref_ = jwt_mgr.get(); // Save reference before moving

    // Initialize API Key manager
    auto api_key_mgr = kj::heap<gateway::ApiKeyManager>();
    api_key_manager_ref_ = api_key_mgr.get(); // Save reference before moving

    // Initialize RBAC manager
    rbac_manager_ = kj::heap<gateway::auth::RbacManager>();

    // Initialize unified auth manager
    // Note: AuthManager takes ownership of jwt_manager and api_key_manager
    auth_manager_ = kj::heap<gateway::AuthManager>(kj::mv(jwt_mgr), kj::mv(api_key_mgr));

    KJ_LOG(INFO, "Authentication initialized", "auth_enabled", config_.auth_enabled, "jwt_expiry",
           config_.jwt_access_expiry_seconds);
  }

  void initializeMiddleware() {
    // Initialize Rate Limiter
    gateway::middleware::RateLimiterConfig rate_limit_config;
    rate_limit_config.capacity = config_.rate_limit_capacity;
    rate_limit_config.refill_rate = config_.rate_limit_refill_rate;
    rate_limit_config.per_user_limiting = config_.rate_limit_per_user;
    rate_limiter_ = kj::heap<gateway::middleware::RateLimiter>(rate_limit_config);

    // Initialize CORS Middleware
    gateway::CorsMiddleware::Config cors_config;
    cors_config.allowedOrigin = kj::heapString(config_.cors_allowed_origin);
    cors_config.allowCredentials = config_.cors_allow_credentials;
    cors_config.maxAge = config_.cors_max_age;
    // Add common methods
    cors_config.allowedMethods.add(kj::str("GET"));
    cors_config.allowedMethods.add(kj::str("POST"));
    cors_config.allowedMethods.add(kj::str("PUT"));
    cors_config.allowedMethods.add(kj::str("DELETE"));
    cors_config.allowedMethods.add(kj::str("OPTIONS"));
    // Add common headers
    cors_config.allowedHeaders.add(kj::str("Content-Type"));
    cors_config.allowedHeaders.add(kj::str("Authorization"));
    cors_config.allowedHeaders.add(kj::str("X-API-Key"));

    cors_middleware_ = kj::heap<gateway::CorsMiddleware>(kj::mv(cors_config));

    // Initialize Auth Middleware (only if auth is enabled)
    // Note: AuthMiddleware takes ownership of AuthManager
    if (config_.auth_enabled) {
      gateway::AuthMiddleware::Config auth_config;
      auth_config.require_auth = true;
      // Public paths that don't require authentication
      auth_config.public_paths.add(kj::str("/health"));
      auth_config.public_paths.add(kj::str("/api/health"));
      auth_config.public_paths.add(kj::str("/api/auth/login"));
      auth_config.public_paths.add(kj::str("/api/auth/refresh"));

      auth_middleware_ = kj::heap<gateway::AuthMiddleware>(
          kj::mv(auth_manager_), audit_logger_.get(), kj::mv(auth_config));
    }

    // Initialize Audit Middleware
    audit_middleware_ = kj::heap<gateway::AuditMiddleware>(audit_logger_.get());

    // Initialize Metrics Middleware
    metrics_middleware_ = kj::heap<gateway::MetricsMiddleware>(*metrics_registry_);

    KJ_LOG(INFO, "Middleware chain initialized", "rate_limit_capacity", config_.rate_limit_capacity,
           "cors_origin", config_.cors_allowed_origin);
  }

  kj::Promise<void> initializeEngineBridge() {
    // Configure engine bridge
    gateway::bridge::EngineBridgeConfig bridge_config;
    bridge_config.event_queue_capacity = config_.event_queue_capacity;
    bridge_config.max_subscriptions = config_.max_event_subscriptions;
    bridge_config.enable_metrics = true;

    engine_bridge_ = kj::heap<gateway::bridge::EngineBridge>(kj::mv(bridge_config));

    // Initialize and start the bridge
    co_await engine_bridge_->initialize(io_);
    co_await engine_bridge_->start();

    KJ_LOG(INFO, "Engine Bridge initialized", "preset", config_.engine_preset);
  }

  void initializeEventBroadcaster() {
    // Configure event broadcaster
    gateway::bridge::EventBroadcasterConfig broadcaster_config;
    broadcaster_config.history_size = 500;
    broadcaster_config.keepalive_interval_ms = config_.sse_keepalive_interval_ms;
    broadcaster_config.max_subscriptions = config_.sse_max_concurrent_streams;

    event_broadcaster_ = kj::heap<gateway::bridge::EventBroadcaster>(broadcaster_config);

    KJ_LOG(INFO, "Event Broadcaster initialized");
  }

  void initializeHandlers() {
    // Initialize Health Handler
    health_handler_ = kj::heap<gateway::HealthHandler>(*engine_bridge_);

    // Initialize Auth Handler
    auth_handler_ =
        kj::heap<gateway::AuthHandler>(*jwt_manager_ref_, *api_key_manager_ref_, *audit_logger_);

    // Initialize SSE Handler
    gateway::handlers::SseHandlerConfig sse_config;
    sse_config.keepalive_interval_ms = config_.sse_keepalive_interval_ms;
    sse_config.retry_ms = config_.sse_retry_ms;
    sse_config.max_concurrent_streams = config_.sse_max_concurrent_streams;

    sse_handler_ = kj::heap<gateway::handlers::SseHandler>(*event_broadcaster_, sse_config);

    // Initialize Market Handler
    market_handler_ = kj::heap<gateway::MarketHandler>(engine_bridge_.get());

    // Initialize Order Handler
    order_handler_ = kj::heap<gateway::OrderHandler>(engine_bridge_.get(), audit_logger_.get());

    // Initialize Account Handler
    account_handler_ = kj::heap<gateway::AccountHandler>(*engine_bridge_, *audit_logger_);

    // Initialize Config Handler
    config_handler_ = kj::heap<gateway::ConfigHandler>(*audit_logger_);
    config_handler_->initialize_defaults();

    // Initialize Audit Handler
    // Note: AuditHandler requires AuditStore, not AuditLogger
    // For now, we'll skip this until AuditStore is implemented
    // audit_handler_ = kj::heap<gateway::AuditHandler>(*audit_store_);

    // Initialize Metrics Handler
    metrics_handler_ = kj::heap<gateway::MetricsHandler>(*metrics_registry_);

    // Initialize Static File Server
    gateway::StaticFileServer::Config static_config;
    static_config.staticDir = kj::heapString(config_.static_dir);
    static_config.enableCache = config_.static_cache_enabled;
    static_config.maxAge = config_.static_cache_max_age;

    static_file_server_ = kj::heap<gateway::StaticFileServer>(static_config);

    // Initialize Static Handler
    static_handler_ = kj::heap<gateway::StaticHandler>(*static_file_server_);

    KJ_LOG(INFO, "Request handlers initialized", "static_dir", config_.static_dir);
  }

  void initializeRouter() {
    // Initialize HTTP header table
    header_table_ = kj::heap<kj::HttpHeaderTable>();

    // Initialize Router
    router_ = kj::heap<gateway::Router>();

    // Register all routes
    registerRoutes();

    KJ_LOG(INFO, "Router initialized with", router_->route_count(), "routes");
  }

  // ===========================================================================
  // Route Registration
  // ===========================================================================

  void registerRoutes() {
    // Health endpoints (public)
    router_->add_route(kj::HttpMethod::GET, "/health",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return health_handler_->handleSimpleHealth(ctx);
                       });

    router_->add_route(kj::HttpMethod::GET, "/api/health",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return health_handler_->handleDetailedHealth(ctx);
                       });

    // Authentication endpoints (public for login/refresh)
    router_->add_route(kj::HttpMethod::POST, "/api/auth/login",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return auth_handler_->handleLogin(ctx);
                       });

    router_->add_route(kj::HttpMethod::POST, "/api/auth/refresh",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return auth_handler_->handleRefresh(ctx);
                       });

    router_->add_route(kj::HttpMethod::POST, "/api/auth/logout",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return auth_handler_->handleLogout(ctx);
                       });

    // API Key management (requires auth)
    router_->add_route(kj::HttpMethod::GET, "/api/auth/keys",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return auth_handler_->handleListApiKeys(ctx);
                       });

    router_->add_route(kj::HttpMethod::POST, "/api/auth/keys",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return auth_handler_->handleCreateApiKey(ctx);
                       });

    router_->add_route(kj::HttpMethod::DELETE, "/api/auth/keys/{id}",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         kj::StringPtr key_id = "";
                         KJ_IF_SOME(id, ctx.path_params.find("id"_kj)) {
                           key_id = id;
                         }
                         return auth_handler_->handleRevokeApiKey(ctx, key_id);
                       });

    // Order management endpoints
    router_->add_route(kj::HttpMethod::POST, "/api/orders",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return order_handler_->handleSubmitOrder(ctx);
                       });

    router_->add_route(kj::HttpMethod::GET, "/api/orders",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return order_handler_->handleListOrders(ctx);
                       });

    router_->add_route(kj::HttpMethod::GET, "/api/orders/{id}",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return order_handler_->handleGetOrder(ctx);
                       });

    router_->add_route(kj::HttpMethod::DELETE, "/api/orders/{id}",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return order_handler_->handleCancelOrder(ctx);
                       });

    router_->add_route(kj::HttpMethod::POST, "/api/cancel",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return order_handler_->handleBulkCancel(ctx);
                       });

    // Account endpoints
    router_->add_route(kj::HttpMethod::GET, "/api/account",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return account_handler_->handleGetAccount(ctx);
                       });

    router_->add_route(kj::HttpMethod::GET, "/api/account/positions",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return account_handler_->handleGetPositions(ctx);
                       });

    router_->add_route(kj::HttpMethod::GET, "/api/account/positions/{symbol}",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return account_handler_->handleGetPosition(ctx);
                       });

    // Configuration endpoints
    router_->add_route(kj::HttpMethod::GET, "/api/config",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return config_handler_->handleGetConfig(ctx);
                       });

    router_->add_route(kj::HttpMethod::GET, "/api/config/{key}",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return config_handler_->handleGetConfigKey(ctx);
                       });

    router_->add_route(kj::HttpMethod::POST, "/api/config",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return config_handler_->handleUpdateConfig(ctx);
                       });

    router_->add_route(kj::HttpMethod::POST, "/api/config/{key}",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return config_handler_->handleUpdateConfigKey(ctx);
                       });

    router_->add_route(kj::HttpMethod::DELETE, "/api/config/{key}",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return config_handler_->handleDeleteConfigKey(ctx);
                       });

    // Metrics endpoint (Prometheus)
    router_->add_route(kj::HttpMethod::GET, "/metrics",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return metrics_handler_->handleMetrics(ctx);
                       });

    // SSE streaming endpoint
    router_->add_route(kj::HttpMethod::GET, "/api/stream",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         // SSE handler has a different signature, we need to adapt
                         return sse_handler_->handle(ctx.method, ctx.path, ctx.headers, ctx.body,
                                                     ctx.response);
                       });

    // Market data endpoints
    router_->add_route(kj::HttpMethod::GET, "/api/market",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return market_handler_->handleGetMarket(ctx);
                       });

    router_->add_route(kj::HttpMethod::GET, "/api/market/{symbol}",
                       [this](gateway::RequestContext& ctx) -> kj::Promise<void> {
                         return market_handler_->handleGetMarket(ctx);
                       });

    // Static files - catch-all for non-API routes
    // Note: This should be registered last as a fallback
    // The router should handle this as a default handler
  }

  // ===========================================================================
  // Cleanup Methods
  // ===========================================================================

  void stopHttpServer() {
    // The HTTP server will be stopped when the promise completes
    // or when the listener is destroyed
    listener_ = kj::none;
    KJ_LOG(INFO, "HTTP server stopped");
  }

  void closeSseConnections() {
    // Event broadcaster subscriptions are cleaned up automatically
    // when the broadcaster is destroyed
    if (event_broadcaster_) {
      KJ_LOG(INFO, "SSE connections closing", "active_subscriptions",
             event_broadcaster_->subscription_count());
    }
  }

  void stopEngineBridge() {
    if (engine_bridge_) {
      engine_bridge_->stop();
      KJ_LOG(INFO, "Engine Bridge stopped");
    }
  }

  void flushAuditLogs() {
    if (audit_logger_) {
      // Flush synchronously - this is called during shutdown
      auto flushPromise = audit_logger_->flush();
      flushPromise.wait(io_.waitScope);
      KJ_LOG(INFO, "Audit Logs flushed");
    }
  }

  void cleanupMiddleware() {
    if (rate_limiter_) {
      rate_limiter_->cleanup_stale_buckets();
    }
  }

  // ===========================================================================
  // Member Variables
  // ===========================================================================

  const GatewayConfig& config_;
  kj::AsyncIoContext& io_;
  std::atomic<bool> cleaned_up_{false};

  // HTTP infrastructure
  kj::Own<kj::HttpHeaderTable> header_table_;
  kj::Maybe<kj::Own<kj::ConnectionReceiver>> listener_;
  kj::Own<gateway::Router> router_;

  // Core components (no dependencies)
  core::MetricsRegistry* metrics_registry_ = nullptr;

  // Audit (no dependencies)
  kj::Own<gateway::audit::AuditLogger> audit_logger_;

  // Authentication components
  gateway::auth::JwtManager* jwt_manager_ref_ = nullptr;  // Non-owning reference for handlers
  gateway::ApiKeyManager* api_key_manager_ref_ = nullptr; // Non-owning reference for handlers
  kj::Own<gateway::auth::RbacManager> rbac_manager_;
  kj::Own<gateway::AuthManager> auth_manager_;

  // Middleware components
  kj::Own<gateway::middleware::RateLimiter> rate_limiter_;
  kj::Own<gateway::CorsMiddleware> cors_middleware_;
  kj::Own<gateway::AuthMiddleware> auth_middleware_;
  kj::Own<gateway::AuditMiddleware> audit_middleware_;
  kj::Own<gateway::MetricsMiddleware> metrics_middleware_;

  // Bridge components
  kj::Own<gateway::bridge::EngineBridge> engine_bridge_;
  kj::Own<gateway::bridge::EventBroadcaster> event_broadcaster_;

  // Request handlers
  kj::Own<gateway::HealthHandler> health_handler_;
  kj::Own<gateway::AuthHandler> auth_handler_;
  kj::Own<gateway::handlers::SseHandler> sse_handler_;
  kj::Own<gateway::MarketHandler> market_handler_;
  kj::Own<gateway::OrderHandler> order_handler_;
  kj::Own<gateway::AccountHandler> account_handler_;
  kj::Own<gateway::ConfigHandler> config_handler_;
  kj::Own<gateway::AuditHandler> audit_handler_;
  kj::Own<gateway::MetricsHandler> metrics_handler_;
  kj::Own<gateway::StaticFileServer> static_file_server_;
  kj::Own<gateway::StaticHandler> static_handler_;
};

// ============================================================================
// Main Entry Point
// ============================================================================

} // namespace veloz

/**
 * @brief Main entry point for VeloZ Gateway
 *
 * Performs the following steps:
 * 1. Load configuration from environment variables
 * 2. Validate configuration
 * 3. Set up signal handlers for graceful shutdown
 * 4. Create async I/O context (KJ EventLoop)
 * 5. Initialize components in dependency order
 * 6. Run HTTP server
 * 7. Handle shutdown gracefully
 * 8. Exit with appropriate status code
 *
 * @param argc Argument count (unused, reserved for future use)
 * @param argv Argument values (unused, reserved for future use)
 * @return 0 on success, 1 on error
 */
int main(int argc, char** argv) {
  using namespace veloz;

  try {
    // Step 1: Load configuration from environment
    auto config = GatewayConfig::loadFromEnv();

    // Step 2: Validate configuration
    config.validate();

    // Log startup banner
    KJ_LOG(INFO, "========================================");
    KJ_LOG(INFO, "  VeloZ Gateway Starting");
    KJ_LOG(INFO, "========================================");
    KJ_LOG(INFO, "Configuration:", "host", config.host, "port", config.port, "auth_enabled",
           config.auth_enabled, "rate_limit_capacity", config.rate_limit_capacity, "static_dir",
           config.static_dir);

    // Step 3: Set up signal handlers for graceful shutdown
    // SIGTERM: Standard termination signal (e.g., from systemd, Docker, Kubernetes)
    // SIGINT: Interrupt signal (e.g., Ctrl+C from terminal)
    std::signal(SIGTERM, signal_handler);
    std::signal(SIGINT, signal_handler);
    KJ_LOG(INFO, "Signal handlers registered (SIGTERM, SIGINT)");

    // Step 4: Create async I/O context (KJ EventLoop)
    auto io = kj::setupAsyncIo();
    KJ_LOG(INFO, "KJ EventLoop initialized");

    // Step 5: Create lifecycle manager
    auto lifecycle = kj::heap<GatewayLifecycle>(config, io);

    // Step 6: Initialize components in dependency order
    auto initPromise = lifecycle->initialize();

    try {
      initPromise.wait(io.waitScope);
    } catch (const kj::Exception& e) {
      KJ_LOG(ERROR, "Component initialization failed", e.getDescription());
      return 1;
    }

    // Step 7: Run server
    KJ_LOG(INFO, "Press Ctrl+C to stop the server");

    auto runPromise = lifecycle->run();

    try {
      runPromise.wait(io.waitScope);
    } catch (const kj::Exception& e) {
      KJ_LOG(ERROR, "Server error", e.getDescription());
    }

    // Step 8: Cleanup handled by lifecycle destructor
    KJ_LOG(INFO, "Gateway shutdown complete");
    return 0;

  } catch (const kj::Exception& e) {
    // KJ framework exception
    KJ_LOG(ERROR, "Fatal KJ exception", e.getDescription());
    return 1;
  } catch (const std::exception& e) {
    // Standard exception
    KJ_LOG(ERROR, "Fatal std::exception", e.what());
    return 1;
  } catch (...) {
    // Unknown exception
    KJ_LOG(ERROR, "Fatal unknown exception");
    return 1;
  }
}
