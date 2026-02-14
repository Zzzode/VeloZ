#include "veloz/engine/stdio_engine.h"

#include "veloz/core/logger.h"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/string.h>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>

namespace veloz {
namespace engine {

namespace {
// Helper to extract value from kj::Maybe with a default
template <typename T> T maybe_or(const kj::Maybe<T>& maybe, T default_value) {
  T result = default_value;
  KJ_IF_MAYBE (val, maybe) {
    result = *val;
  }
  return result;
}
} // namespace

StdioEngine::StdioEngine(std::ostream& out) : out_(out), output_mutex_(0), command_count_(0) {}

void StdioEngine::emit_event(kj::StringPtr event_json) {
  auto lock = output_mutex_.lockExclusive();
  out_ << event_json.cStr() << std::endl;
  out_.flush();
}

void StdioEngine::emit_error(kj::StringPtr error_msg) {
  std::ostringstream oss;
  oss << R"({
        "type": "error",
        "message": ")"
      << error_msg.cStr() << R"("
    })";
  emit_event(kj::StringPtr(oss.str().c_str()));
}

int StdioEngine::run(std::atomic<bool>& stop_flag) {
  std::cout << "VeloZ StdioEngine started - press Ctrl+C to stop" << std::endl;
  std::cout << "Commands: ORDER <SIDE> <SYMBOL> <QTY> <PRICE> <ID> [TYPE] [TIF]" << std::endl;
  std::cout << "          CANCEL <ID>" << std::endl;
  std::cout << "          QUERY <TYPE> [PARAMS]" << std::endl;
  std::cout << std::endl;

  std::string line;

  // Send startup event
  emit_event(R"({
        "type": "engine_started",
        "version": "1.0.0"
    })"_kj);

  while (!stop_flag.load() && std::getline(std::cin, line)) {
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
      continue;
    }

    command_count_++;

    // Parse the command
    ParsedCommand command = parse_command(kj::StringPtr(line.c_str()));

    switch (command.type) {
    case CommandType::Order:
      KJ_IF_MAYBE (order, command.order) {
        // Emit order received event
        std::ostringstream oss;
        oss << R"({
                        "type": "order_received",
                        "command_id": )"
            << command_count_ << R"(,
                        "client_order_id": ")"
            << order->request.client_order_id.cStr() << R"(",
                        "symbol": ")"
            << order->request.symbol.value << R"(",
                        "side": ")"
            << (order->request.side == veloz::exec::OrderSide::Buy ? "buy" : "sell") << R"(",
                        "type": ")"
            << (order->request.type == veloz::exec::OrderType::Market ? "market" : "limit") << R"(",
                        "quantity": )"
            << order->request.qty << R"(,
                        "price": )"
            << maybe_or(order->request.price, 0.0) << R"(
                    })";
        emit_event(kj::StringPtr(oss.str().c_str()));

        // Call handler if registered
        KJ_IF_MAYBE (handler, order_handler_) {
          (*handler)(*order);
        }
      } else {
        emit_error(kj::str("Failed to parse ORDER command: ", command.error));
      }
      break;

    case CommandType::Cancel:
      KJ_IF_MAYBE (cancel, command.cancel) {
        // Emit cancel received event
        std::ostringstream oss;
        oss << R"({
                        "type": "cancel_received",
                        "command_id": )"
            << command_count_ << R"(,
                        "client_order_id": ")"
            << cancel->client_order_id.cStr() << R"("
                    })";
        emit_event(kj::StringPtr(oss.str().c_str()));

        // Call handler if registered
        KJ_IF_MAYBE (handler, cancel_handler_) {
          (*handler)(*cancel);
        }
      } else {
        emit_error(kj::str("Failed to parse CANCEL command: ", command.error));
      }
      break;

    case CommandType::Query:
      KJ_IF_MAYBE (query, command.query) {
        // Emit query received event
        std::ostringstream oss;
        oss << R"({
                        "type": "query_received",
                        "command_id": )"
            << command_count_ << R"(,
                        "query_type": ")"
            << query->query_type.cStr() << R"(",
                        "params": ")"
            << query->params.cStr() << R"("
                    })";
        emit_event(kj::StringPtr(oss.str().c_str()));

        // Call handler if registered
        KJ_IF_MAYBE (handler, query_handler_) {
          (*handler)(*query);
        }
      } else {
        emit_error(kj::str("Failed to parse QUERY command: ", command.error));
      }
      break;

    case CommandType::Unknown:
      if (command.error.size() > 0) {
        emit_error(command.error);
      }
      break;
    }
  }

  // Send shutdown event
  {
    std::ostringstream oss;
    oss << R"({
        "type": "engine_stopped",
        "commands_processed": )"
        << command_count_ << R"(
    })";
    emit_event(kj::StringPtr(oss.str().c_str()));
  }

  return 0;
}

} // namespace engine
} // namespace veloz
