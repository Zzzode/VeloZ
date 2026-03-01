#include "veloz/engine/http_service.h"

#include "veloz/strategy/strategy.h"

#include <kj/debug.h>
#include <kj/string.h>

namespace veloz::engine {

namespace {

kj::StringPtr lifecycle_state_to_string(EngineLifecycleState state) {
  switch (state) {
  case EngineLifecycleState::Starting:
    return "starting"_kj;
  case EngineLifecycleState::Running:
    return "running"_kj;
  case EngineLifecycleState::Stopping:
    return "stopping"_kj;
  case EngineLifecycleState::Stopped:
    return "stopped"_kj;
  }
  return "unknown"_kj;
}

kj::StringPtr strategy_state_to_string(const veloz::strategy::StrategyState& state) {
  // Convert StrategyState struct to a status string based on is_running field
  if (state.is_running) {
    return "running"_kj;
  }
  return "stopped"_kj;
}

// Check if URL starts with a prefix and extract the remaining path
bool url_starts_with(kj::StringPtr url, kj::StringPtr prefix, kj::StringPtr& remaining) {
  if (url.size() >= prefix.size() && url.slice(0, prefix.size()) == prefix) {
    remaining = url.slice(prefix.size());
    return true;
  }
  return false;
}

} // namespace

// ============================================================================
// EngineHttpService Implementation
// ============================================================================

EngineHttpService::EngineHttpService(const kj::HttpHeaderTable& headerTable,
                                     kj::MutexGuarded<bool>& stopFlag)
    : headerTable_(headerTable), stopFlag_(stopFlag), engineState_(EngineLifecycleState::Starting) {
}

kj::Promise<void> EngineHttpService::request(kj::HttpMethod method, kj::StringPtr url,
                                             const kj::HttpHeaders& headers,
                                             kj::AsyncInputStream& requestBody,
                                             Response& response) {
  (void)headers;
  (void)requestBody;

  if (url == "/api/control/status"_kj) {
    if (method == kj::HttpMethod::GET) {
      return handleStatus(response);
    }
    return handleMethodNotAllowed(response);
  }

  if (url == "/api/control/health"_kj) {
    if (method == kj::HttpMethod::GET) {
      return handleHealth(response);
    }
    return handleMethodNotAllowed(response);
  }

  if (url == "/api/control/config"_kj) {
    if (method == kj::HttpMethod::GET) {
      return handleConfig(response);
    }
    return handleMethodNotAllowed(response);
  }

  if (url == "/api/control/start"_kj) {
    if (method == kj::HttpMethod::POST) {
      return handleStart(response);
    }
    return handleMethodNotAllowed(response);
  }

  if (url == "/api/control/stop"_kj) {
    if (method == kj::HttpMethod::POST) {
      return handleStop(response);
    }
    return handleMethodNotAllowed(response);
  }

  if (url == "/api/control/strategies"_kj) {
    if (method == kj::HttpMethod::GET) {
      return handleListStrategies(response);
    }
    return handleMethodNotAllowed(response);
  }

  kj::StringPtr controlRemaining;
  if (url_starts_with(url, "/api/control/strategies/"_kj, controlRemaining)) {
    auto slashPos = controlRemaining.findFirst('/');
    kj::StringPtr strategyId;
    kj::StringPtr action;

    KJ_IF_SOME(pos, slashPos) {
      strategyId = kj::StringPtr(controlRemaining.begin(), pos);
      action = kj::StringPtr(controlRemaining.begin() + pos + 1, controlRemaining.size() - pos - 1);
    }
    else {
      strategyId = controlRemaining;
      action = ""_kj;
    }

    if (strategyId.size() == 0) {
      return handleNotFound(response);
    }

    if (action.size() == 0) {
      if (method == kj::HttpMethod::GET) {
        return handleGetStrategy(strategyId, response);
      }
      return handleMethodNotAllowed(response);
    }

    if (action == "start"_kj) {
      if (method == kj::HttpMethod::POST) {
        return handleStartStrategy(strategyId, response);
      }
      return handleMethodNotAllowed(response);
    }

    if (action == "stop"_kj) {
      if (method == kj::HttpMethod::POST) {
        return handleStopStrategy(strategyId, response);
      }
      return handleMethodNotAllowed(response);
    }
  }

  // Route requests based on URL and method
  if (url == "/api/status"_kj) {
    if (method == kj::HttpMethod::GET) {
      return handleStatus(response);
    }
    return handleMethodNotAllowed(response);
  }

  if (url == "/api/health"_kj) {
    if (method == kj::HttpMethod::GET) {
      return handleHealth(response);
    }
    return handleMethodNotAllowed(response);
  }

  if (url == "/api/start"_kj) {
    if (method == kj::HttpMethod::POST) {
      return handleStart(response);
    }
    return handleMethodNotAllowed(response);
  }

  if (url == "/api/stop"_kj) {
    if (method == kj::HttpMethod::POST) {
      return handleStop(response);
    }
    return handleMethodNotAllowed(response);
  }

  // Strategy endpoints
  if (url == "/api/strategies"_kj) {
    if (method == kj::HttpMethod::GET) {
      return handleListStrategies(response);
    }
    return handleMethodNotAllowed(response);
  }

  // Handle /api/strategies/{id} and /api/strategies/{id}/start|stop
  kj::StringPtr remaining;
  if (url_starts_with(url, "/api/strategies/"_kj, remaining)) {
    // Parse strategy ID and optional action
    auto slashPos = remaining.findFirst('/');
    kj::StringPtr strategyId;
    kj::StringPtr action;

    KJ_IF_SOME(pos, slashPos) {
      strategyId = kj::StringPtr(remaining.begin(), pos);
      action = kj::StringPtr(remaining.begin() + pos + 1, remaining.size() - pos - 1);
    }
    else {
      strategyId = remaining;
      action = ""_kj;
    }

    if (strategyId.size() == 0) {
      return handleNotFound(response);
    }

    if (action.size() == 0) {
      // GET /api/strategies/{id}
      if (method == kj::HttpMethod::GET) {
        return handleGetStrategy(strategyId, response);
      }
      return handleMethodNotAllowed(response);
    }

    if (action == "start"_kj) {
      // POST /api/strategies/{id}/start
      if (method == kj::HttpMethod::POST) {
        return handleStartStrategy(strategyId, response);
      }
      return handleMethodNotAllowed(response);
    }

    if (action == "stop"_kj) {
      // POST /api/strategies/{id}/stop
      if (method == kj::HttpMethod::POST) {
        return handleStopStrategy(strategyId, response);
      }
      return handleMethodNotAllowed(response);
    }
  }

  return handleNotFound(response);
}

void EngineHttpService::set_engine_state(EngineLifecycleState state) {
  *engineState_.lockExclusive() = state;
}

EngineLifecycleState EngineHttpService::get_engine_state() const {
  return *engineState_.lockShared();
}

void EngineHttpService::set_start_callback(StartCallback callback) {
  startCallback_ = kj::mv(callback);
}

void EngineHttpService::set_stop_callback(StopCallback callback) {
  stopCallback_ = kj::mv(callback);
}

void EngineHttpService::set_strategy_manager(veloz::strategy::StrategyManager* manager) {
  strategyManager_ = manager;
}

kj::Promise<void> EngineHttpService::handleStatus(Response& response) {
  kj::String body = buildStatusJson();
  kj::HttpHeaders responseHeaders(headerTable_);
  responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

  auto stream = response.send(200, "OK"_kj, responseHeaders, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream), kj::mv(body));
}

kj::Promise<void> EngineHttpService::handleHealth(Response& response) {
  kj::String body = buildHealthJson();
  kj::HttpHeaders responseHeaders(headerTable_);
  responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

  auto stream = response.send(200, "OK"_kj, responseHeaders, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream), kj::mv(body));
}

kj::Promise<void> EngineHttpService::handleConfig(Response& response) {
  kj::String body = buildConfigJson();
  kj::HttpHeaders responseHeaders(headerTable_);
  responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

  auto stream = response.send(200, "OK"_kj, responseHeaders, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream), kj::mv(body));
}

kj::Promise<void> EngineHttpService::handleStart(Response& response) {
  auto currentState = get_engine_state();
  kj::String body;
  unsigned int statusCode = 200;
  kj::StringPtr statusText = "OK"_kj;

  if (currentState == EngineLifecycleState::Running) {
    body = buildSuccessJson("Engine is already running"_kj);
  } else if (currentState == EngineLifecycleState::Stopping ||
             currentState == EngineLifecycleState::Stopped) {
    statusCode = 400;
    statusText = "Bad Request"_kj;
    body = buildErrorJson("Cannot start engine in current state"_kj);
  } else {
    // Try to start via callback
    bool started = false;
    KJ_IF_SOME(callback, startCallback_) {
      started = callback();
    }
    else {
      // No callback, just set state to running
      set_engine_state(EngineLifecycleState::Running);
      started = true;
    }

    if (started) {
      body = buildSuccessJson("Engine started"_kj);
    } else {
      statusCode = 500;
      statusText = "Internal Server Error"_kj;
      body = buildErrorJson("Failed to start engine"_kj);
    }
  }

  kj::HttpHeaders responseHeaders(headerTable_);
  responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

  auto stream = response.send(statusCode, statusText, responseHeaders, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream), kj::mv(body));
}

kj::Promise<void> EngineHttpService::handleStop(Response& response) {
  auto currentState = get_engine_state();
  kj::String body;
  unsigned int statusCode = 200;
  kj::StringPtr statusText = "OK"_kj;

  if (currentState == EngineLifecycleState::Stopped) {
    body = buildSuccessJson("Engine is already stopped"_kj);
  } else if (currentState == EngineLifecycleState::Stopping) {
    body = buildSuccessJson("Engine is already stopping"_kj);
  } else {
    // Set stopping state
    set_engine_state(EngineLifecycleState::Stopping);

    // Try to stop via callback
    bool stopped = false;
    KJ_IF_SOME(callback, stopCallback_) {
      stopped = callback();
    }
    else {
      // No callback, just set the stop flag
      *stopFlag_.lockExclusive() = true;
      stopped = true;
    }

    if (stopped) {
      body = buildSuccessJson("Engine stopping"_kj);
    } else {
      statusCode = 500;
      statusText = "Internal Server Error"_kj;
      body = buildErrorJson("Failed to stop engine"_kj);
    }
  }

  kj::HttpHeaders responseHeaders(headerTable_);
  responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

  auto stream = response.send(statusCode, statusText, responseHeaders, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream), kj::mv(body));
}

kj::Promise<void> EngineHttpService::handleNotFound(Response& response) {
  kj::String body = buildErrorJson("Not found"_kj);
  kj::HttpHeaders responseHeaders(headerTable_);
  responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

  auto stream = response.send(404, "Not Found"_kj, responseHeaders, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream), kj::mv(body));
}

kj::Promise<void> EngineHttpService::handleMethodNotAllowed(Response& response) {
  kj::String body = buildErrorJson("Method not allowed"_kj);
  kj::HttpHeaders responseHeaders(headerTable_);
  responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

  auto stream = response.send(405, "Method Not Allowed"_kj, responseHeaders, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream), kj::mv(body));
}

kj::Promise<void> EngineHttpService::handleListStrategies(Response& response) {
  kj::String body;
  unsigned int statusCode = 200;
  kj::StringPtr statusText = "OK"_kj;

  if (strategyManager_ == nullptr) {
    statusCode = 503;
    statusText = "Service Unavailable"_kj;
    body = buildErrorJson("Strategy manager not initialized"_kj);
  } else {
    // Build JSON array of strategies
    auto strategies = strategyManager_->get_all_strategy_ids();
    kj::Vector<kj::String> items;
    for (size_t i = 0; i < strategies.size(); ++i) {
      auto strategy = strategyManager_->get_strategy(strategies[i]);
      if (strategy != nullptr) {
        auto state = strategy->get_state();
        items.add(kj::str(R"({"id":")", strategies[i], R"(","state":")",
                          strategy_state_to_string(state), R"("})"));
      }
    }

    if (items.size() == 0) {
      body = kj::str(R"({"strategies":[]})");
    } else {
      // Manually join items with comma
      kj::String joined = kj::str("");
      for (size_t i = 0; i < items.size(); ++i) {
        if (i > 0) {
          joined = kj::str(joined, ",", items[i]);
        } else {
          joined = kj::str(items[i]);
        }
      }
      body = kj::str(R"({"strategies":[)", joined, R"(]})");
    }
  }

  kj::HttpHeaders responseHeaders(headerTable_);
  responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

  auto stream = response.send(statusCode, statusText, responseHeaders, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream), kj::mv(body));
}

kj::Promise<void> EngineHttpService::handleGetStrategy(kj::StringPtr strategyId,
                                                       Response& response) {
  kj::String body;
  unsigned int statusCode = 200;
  kj::StringPtr statusText = "OK"_kj;

  if (strategyManager_ == nullptr) {
    statusCode = 503;
    statusText = "Service Unavailable"_kj;
    body = buildErrorJson("Strategy manager not initialized"_kj);
  } else {
    auto strategy = strategyManager_->get_strategy(strategyId);
    if (strategy != nullptr) {
      auto state = strategy->get_state();
      body = kj::str(R"({"id":")", strategyId, R"(","state":")", strategy_state_to_string(state),
                     R"("})");
    } else {
      statusCode = 404;
      statusText = "Not Found"_kj;
      body = buildErrorJson("Strategy not found"_kj);
    }
  }

  kj::HttpHeaders responseHeaders(headerTable_);
  responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

  auto stream = response.send(statusCode, statusText, responseHeaders, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream), kj::mv(body));
}

kj::Promise<void> EngineHttpService::handleStartStrategy(kj::StringPtr strategyId,
                                                         Response& response) {
  kj::String body;
  unsigned int statusCode = 200;
  kj::StringPtr statusText = "OK"_kj;

  if (strategyManager_ == nullptr) {
    statusCode = 503;
    statusText = "Service Unavailable"_kj;
    body = buildErrorJson("Strategy manager not initialized"_kj);
  } else {
    bool started = strategyManager_->start_strategy(strategyId);
    if (started) {
      body = buildSuccessJson("Strategy started"_kj);
    } else {
      statusCode = 400;
      statusText = "Bad Request"_kj;
      body = buildErrorJson("Failed to start strategy"_kj);
    }
  }

  kj::HttpHeaders responseHeaders(headerTable_);
  responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

  auto stream = response.send(statusCode, statusText, responseHeaders, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream), kj::mv(body));
}

kj::Promise<void> EngineHttpService::handleStopStrategy(kj::StringPtr strategyId,
                                                        Response& response) {
  kj::String body;
  unsigned int statusCode = 200;
  kj::StringPtr statusText = "OK"_kj;

  if (strategyManager_ == nullptr) {
    statusCode = 503;
    statusText = "Service Unavailable"_kj;
    body = buildErrorJson("Strategy manager not initialized"_kj);
  } else {
    bool stopped = strategyManager_->stop_strategy(strategyId);
    if (stopped) {
      body = buildSuccessJson("Strategy stopped"_kj);
    } else {
      statusCode = 400;
      statusText = "Bad Request"_kj;
      body = buildErrorJson("Failed to stop strategy"_kj);
    }
  }

  kj::HttpHeaders responseHeaders(headerTable_);
  responseHeaders.setPtr(kj::HttpHeaderId::CONTENT_TYPE, "application/json"_kj);

  auto stream = response.send(statusCode, statusText, responseHeaders, body.size());
  auto writePromise = stream->write(body.asBytes());
  return writePromise.attach(kj::mv(stream), kj::mv(body));
}

kj::String EngineHttpService::buildStatusJson() const {
  auto state = get_engine_state();
  return kj::str(R"({"status":")", lifecycle_state_to_string(state), R"(","version":"1.0.0"})");
}

kj::String EngineHttpService::buildHealthJson() const {
  auto state = get_engine_state();
  bool healthy = (state == EngineLifecycleState::Running);
  return kj::str(R"({"healthy":)", healthy ? "true"_kj : "false"_kj, R"(,"status":")",
                 lifecycle_state_to_string(state), R"("})");
}

kj::String EngineHttpService::buildConfigJson() const {
  return kj::str(R"({"config":{}})");
}

kj::String EngineHttpService::buildSuccessJson(kj::StringPtr message) const {
  return kj::str(R"({"success":true,"message":")", message, R"("})");
}

kj::String EngineHttpService::buildErrorJson(kj::StringPtr error) const {
  return kj::str(R"({"success":false,"error":")", error, R"("})");
}

// ============================================================================
// EngineHttpServer Implementation
// ============================================================================

EngineHttpServer::EngineHttpServer(kj::Timer& timer, kj::Own<EngineHttpService> service,
                                   uint16_t port)
    : headerTable_(kj::heap<kj::HttpHeaderTable>()), service_(kj::mv(service)), port_(port) {
  kj::HttpServerSettings settings;
  settings.headerTimeout = 30 * kj::SECONDS;
  settings.pipelineTimeout = 5 * kj::SECONDS;

  server_ = kj::heap<kj::HttpServer>(timer, *headerTable_, *service_, settings);
}

kj::Promise<void> EngineHttpServer::listen(kj::Network& network) {
  return network.parseAddress(kj::str("0.0.0.0:", port_))
      .then([this](kj::Own<kj::NetworkAddress> addr) {
        auto receiver = addr->listen();
        return server_->listenHttp(*receiver).attach(kj::mv(receiver), kj::mv(addr));
      });
}

kj::Promise<void> EngineHttpServer::drain() {
  return server_->drain();
}

EngineHttpService& EngineHttpServer::getService() {
  return *service_;
}

} // namespace veloz::engine
