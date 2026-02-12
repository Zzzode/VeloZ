#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <thread>
#include <ostream>
#include <functional>
#include "veloz/engine/command_parser.h"

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

    void set_order_handler(OrderHandler handler) { order_handler_ = handler; }
    void set_cancel_handler(CancelHandler handler) { cancel_handler_ = handler; }
    void set_query_handler(QueryHandler handler) { query_handler_ = handler; }

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

}
}
