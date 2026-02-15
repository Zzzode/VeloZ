#pragma once

#include "veloz/engine/command_parser.h"

// std::int64_t for fixed-width integers (standard C types, KJ uses these)
#include <cstdint>
#include <kj/async-io.h>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/io.h>
#include <kj/mutex.h>
#include <kj/string.h>

namespace veloz {
namespace engine {

class StdioEngine final {
public:
  explicit StdioEngine(kj::OutputStream& out, kj::InputStream& in);
  ~StdioEngine() = default;

  int run(kj::MutexGuarded<bool>& stop_flag);

  // Command handlers
  using OrderHandler = kj::Function<void(const ParsedOrder&)>;
  using CancelHandler = kj::Function<void(const ParsedCancel&)>;
  using QueryHandler = kj::Function<void(const ParsedQuery&)>;

  void set_order_handler(OrderHandler handler) {
    order_handler_ = kj::mv(handler);
  }
  void set_cancel_handler(CancelHandler handler) {
    cancel_handler_ = kj::mv(handler);
  }
  void set_query_handler(QueryHandler handler) {
    query_handler_ = kj::mv(handler);
  }

private:
  kj::OutputStream& out_;
  kj::InputStream& in_;
  kj::MutexGuarded<int> output_mutex_;
  kj::Maybe<OrderHandler> order_handler_;
  kj::Maybe<CancelHandler> cancel_handler_;
  kj::Maybe<QueryHandler> query_handler_;
  std::int64_t command_count_;

  void emit_event(kj::StringPtr event_json);
  void emit_error(kj::StringPtr error_msg);
};

} // namespace engine
} // namespace veloz
