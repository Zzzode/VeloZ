#pragma once

#include <kj/async-io.h>
#include <kj/compat/http.h>
#include <kj/function.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>

namespace veloz::strategy {
class StrategyManager;
}

namespace veloz::engine {

/**
 * @brief Engine lifecycle state
 */
enum class EngineLifecycleState {
  Starting, ///< Engine is starting up
  Running,  ///< Engine is running normally
  Stopping, ///< Engine is shutting down gracefully
  Stopped   ///< Engine has stopped
};

/**
 * @brief HTTP service for engine control via REST API
 *
 * Implements kj::HttpService to provide REST endpoints for:
 * - GET /api/status - Engine status
 * - GET /api/health - Health check
 * - POST /api/start - Start engine
 * - POST /api/stop - Stop engine (graceful shutdown)
 * - GET /api/strategies - List all strategies
 * - GET /api/strategies/{id} - Get strategy state
 * - POST /api/strategies/{id}/start - Start a strategy
 * - POST /api/strategies/{id}/stop - Stop a strategy
 */
class EngineHttpService final : public kj::HttpService {
public:
  /**
   * @brief Construct HTTP service
   * @param headerTable HTTP header table for response headers
   * @param stopFlag Reference to engine stop flag for shutdown control
   */
  EngineHttpService(const kj::HttpHeaderTable& headerTable, kj::MutexGuarded<bool>& stopFlag);

  ~EngineHttpService() = default;

  // kj::HttpService interface
  kj::Promise<void> request(kj::HttpMethod method, kj::StringPtr url,
                            const kj::HttpHeaders& headers, kj::AsyncInputStream& requestBody,
                            Response& response) override;

  // Engine state accessors
  void set_engine_state(EngineLifecycleState state);
  EngineLifecycleState get_engine_state() const;

  // Callbacks for engine control
  using StartCallback = kj::Function<bool()>;
  using StopCallback = kj::Function<bool()>;

  void set_start_callback(StartCallback callback);
  void set_stop_callback(StopCallback callback);

  // Strategy manager integration
  void set_strategy_manager(veloz::strategy::StrategyManager* manager);

private:
  const kj::HttpHeaderTable& headerTable_;
  kj::MutexGuarded<bool>& stopFlag_;
  kj::MutexGuarded<EngineLifecycleState> engineState_;

  kj::Maybe<StartCallback> startCallback_;
  kj::Maybe<StopCallback> stopCallback_;

  // Strategy manager (non-owning pointer, managed by EngineApp)
  veloz::strategy::StrategyManager* strategyManager_{nullptr};

  // Request handlers
  kj::Promise<void> handleStatus(Response& response);
  kj::Promise<void> handleHealth(Response& response);
  kj::Promise<void> handleStart(Response& response);
  kj::Promise<void> handleStop(Response& response);
  kj::Promise<void> handleNotFound(Response& response);
  kj::Promise<void> handleMethodNotAllowed(Response& response);

  // Strategy handlers
  kj::Promise<void> handleListStrategies(Response& response);
  kj::Promise<void> handleGetStrategy(kj::StringPtr strategyId, Response& response);
  kj::Promise<void> handleStartStrategy(kj::StringPtr strategyId, Response& response);
  kj::Promise<void> handleStopStrategy(kj::StringPtr strategyId, Response& response);

  // JSON response helpers
  kj::String buildStatusJson() const;
  kj::String buildHealthJson() const;
  kj::String buildSuccessJson(kj::StringPtr message) const;
  kj::String buildErrorJson(kj::StringPtr error) const;
};

/**
 * @brief HTTP server wrapper for engine service mode
 *
 * Manages the HTTP server lifecycle and provides graceful shutdown.
 */
class EngineHttpServer final {
public:
  /**
   * @brief Construct HTTP server
   * @param timer KJ timer for timeouts
   * @param service HTTP service to handle requests
   * @param port Port to listen on (default: 8080)
   */
  EngineHttpServer(kj::Timer& timer, kj::Own<EngineHttpService> service, uint16_t port = 8080);

  ~EngineHttpServer() = default;

  // Disable copy and move
  KJ_DISALLOW_COPY_AND_MOVE(EngineHttpServer);

  /**
   * @brief Start listening for connections
   * @param network KJ network for TCP connections
   * @return Promise that completes when server stops
   */
  kj::Promise<void> listen(kj::Network& network);

  /**
   * @brief Gracefully drain and stop the server
   * @return Promise that completes when all connections are closed
   */
  kj::Promise<void> drain();

  /**
   * @brief Get the HTTP service
   * @return Reference to the HTTP service
   */
  EngineHttpService& getService();

private:
  kj::Own<kj::HttpHeaderTable> headerTable_;
  kj::Own<EngineHttpService> service_;
  kj::Own<kj::HttpServer> server_;
  uint16_t port_;
};

} // namespace veloz::engine
