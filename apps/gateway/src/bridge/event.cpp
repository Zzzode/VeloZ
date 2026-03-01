#include "event.h"

#include <cstdint>
#include <kj/common.h>
#include <kj/string.h>

namespace veloz::gateway::bridge {

kj::StringPtr to_string(SseEventType type) {
  switch (type) {
  case SseEventType::MarketData:
    return "market-data"_kj;
  case SseEventType::OrderUpdate:
    return "order-update"_kj;
  case SseEventType::Account:
    return "account"_kj;
  case SseEventType::System:
    return "system"_kj;
  case SseEventType::Error:
    return "error"_kj;
  case SseEventType::KeepAlive:
    return "keepalive"_kj;
  case SseEventType::Unknown:
  default:
    return "unknown"_kj;
  }
}

kj::String SseEvent::format_sse() const {
  // SSE format: id: <id>\nevent: <type>\ndata: <data>\n\n
  auto type_str = to_string(type);
  // Note: data already contains JSON, we prepend "data: " and add newlines
  // The data may contain multiple lines, so we need to prefix each line with "data: "
  // For simplicity, we assume data is a single line JSON object
  return kj::str("id: ", id, "\n", "event: ", type_str, "\n", "data: ", data, "\n\n");
}

kj::String SseEvent::format_sse(uint64_t retry_ms) const {
  auto type_str = to_string(type);
  return kj::str("id: ", id, "\n", "event: ", type_str, "\n", "data: ", data, "\n",
                 "retry: ", retry_ms, "\n\n");
}

SseEvent SseEvent::create_keepalive(uint64_t id) {
  return SseEvent(id, SseEventType::KeepAlive, 0, kj::str("{}"));
}

SseEvent SseEvent::create_market_data(uint64_t id, kj::String data) {
  return SseEvent(id, SseEventType::MarketData,
                  0, // timestamp set by broadcaster
                  kj::mv(data));
}

SseEvent SseEvent::create_order_update(uint64_t id, kj::String data) {
  return SseEvent(id, SseEventType::OrderUpdate,
                  0, // timestamp set by broadcaster
                  kj::mv(data));
}

SseEvent SseEvent::create_error(uint64_t id, kj::String data) {
  return SseEvent(id, SseEventType::Error,
                  0, // timestamp set by broadcaster
                  kj::mv(data));
}

} // namespace veloz::gateway::bridge
