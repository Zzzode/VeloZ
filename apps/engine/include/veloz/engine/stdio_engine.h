#pragma once

#include "veloz/engine/command_parser.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <ostream>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace veloz {
namespace engine {

class StdioEngine final {
public:
  explicit StdioEngine(std::ostream& out);
  ~StdioEngine() = default;

  int run(std::atomic<bool>& stop_flag);

  // Command handlers
  using OrderHandler = std::function<void(const ParsedOrder&)>;
  using CancelHandler = std::function<void(const ParsedCancel&)>;
  using QueryHandler = std::function<void(const ParsedQuery&)>;

  void set_order_handler(OrderHandler handler) {
    order_handler_ = handler;
  }
  void set_cancel_handler(CancelHandler handler) {
    cancel_handler_ = handler;
  }
  void set_query_handler(QueryHandler handler) {
    query_handler_ = handler;
  }

private:
  std::ostream& out_;
  std::mutex output_mutex_;
  OrderHandler order_handler_;
  CancelHandler cancel_handler_;
  QueryHandler query_handler_;
  std::int64_t command_count_;

  void emit_event(const std::string& event_json);
  void emit_error(const std::string& error_msg);
};

} // namespace engine
} // namespace veloz
