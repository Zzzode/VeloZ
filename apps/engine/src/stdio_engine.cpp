#include "veloz/engine/stdio_engine.h"

#include "veloz/core/logger.h"

#include <kj/common.h>
#include <kj/debug.h>
#include <kj/io.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz {
namespace engine {

namespace {
// Helper to extract value from kj::Maybe with a default
template <typename T> T maybe_or(const kj::Maybe<T>& maybe, T default_value) {
  T result = default_value;
  KJ_IF_SOME (val, maybe) {
    result = val;
  }
  return result;
}

// Helper to read a line from kj::InputStream
kj::Maybe<kj::String> read_line(kj::InputStream& in) {
  kj::Vector<char> line;
  kj::byte c;
  while (true) {
    kj::ArrayPtr<kj::byte> buf(&c, 1);
    size_t n = in.tryRead(buf, 1);
    if (n == 0) {
      // EOF
      if (line.size() == 0) {
        return kj::none;
      }
      break;
    }
    if (static_cast<char>(c) == '\n') {
      break;
    }
    if (static_cast<char>(c) != '\r') {
      line.add(static_cast<char>(c));
    }
  }
  line.add('\0');
  return kj::String(line.releaseAsArray());
}
} // namespace

StdioEngine::StdioEngine(kj::OutputStream& out, kj::InputStream& in)
    : out_(out), in_(in), output_mutex_(0), command_count_(0) {}

void StdioEngine::emit_event(kj::StringPtr event_json) {
  auto lock = output_mutex_.lockExclusive();
  kj::String line = kj::str(event_json, "\n");
  out_.write(line.asBytes());
}

void StdioEngine::emit_error(kj::StringPtr error_msg) {
  kj::String json = kj::str(R"({"type":"error","message":")", error_msg, R"("})");
  emit_event(json);
}

int StdioEngine::run(kj::MutexGuarded<bool>& stop_flag) {
  // Write startup messages
  kj::String banner = kj::str(
      "VeloZ StdioEngine started - press Ctrl+C to stop\n",
      "Commands: ORDER <SIDE> <SYMBOL> <QTY> <PRICE> <ID> [TYPE] [TIF]\n",
      "          CANCEL <ID>\n",
      "          QUERY <TYPE> [PARAMS]\n\n");
  out_.write(banner.asBytes());

  // Send startup event
  emit_event(R"({"type":"engine_started","version":"1.0.0"})"_kj);

  while (!*stop_flag.lockShared()) {
    auto maybe_line = read_line(in_);
    KJ_IF_SOME (line, maybe_line) {
      // Skip empty lines and comments
      if (line.size() == 0 || line[0] == '#') {
        continue;
      }

      command_count_++;

      // Parse the command
      ParsedCommand command = parse_command(line);

      switch (command.type) {
      case CommandType::Order:
        KJ_IF_SOME (order, command.order) {
          // Emit order received event
          kj::String json = kj::str(
              R"({"type":"order_received","command_id":)", command_count_,
              R"(,"client_order_id":")", order.request.client_order_id,
              R"(","symbol":")", order.request.symbol.value,
              R"(","side":")", (order.request.side == veloz::exec::OrderSide::Buy ? "buy" : "sell"),
              R"(","type":")", (order.request.type == veloz::exec::OrderType::Market ? "market" : "limit"),
              R"(","quantity":)", order.request.qty,
              R"(,"price":)", maybe_or(order.request.price, 0.0), "}");
          emit_event(json);

          // Call handler if registered
          KJ_IF_SOME (handler, order_handler_) {
            handler(order);
          }
        } else {
          emit_error(kj::str("Failed to parse ORDER command: ", command.error));
        }
        break;

      case CommandType::Cancel:
        KJ_IF_SOME (cancel, command.cancel) {
          // Emit cancel received event
          kj::String json = kj::str(
              R"({"type":"cancel_received","command_id":)", command_count_,
              R"(,"client_order_id":")", cancel.client_order_id, R"("})");
          emit_event(json);

          // Call handler if registered
          KJ_IF_SOME (handler, cancel_handler_) {
            handler(cancel);
          }
        } else {
          emit_error(kj::str("Failed to parse CANCEL command: ", command.error));
        }
        break;

      case CommandType::Query:
        KJ_IF_SOME (query, command.query) {
          // Emit query received event
          kj::String json = kj::str(
              R"({"type":"query_received","command_id":)", command_count_,
              R"(,"query_type":")", query.query_type,
              R"(","params":")", query.params, R"("})");
          emit_event(json);

          // Call handler if registered
          KJ_IF_SOME (handler, query_handler_) {
            handler(query);
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
    } else {
      // EOF reached
      break;
    }
  }

  // Send shutdown event
  kj::String shutdown_json = kj::str(
      R"({"type":"engine_stopped","commands_processed":)", command_count_, "}");
  emit_event(shutdown_json);

  return 0;
}

} // namespace engine
} // namespace veloz
