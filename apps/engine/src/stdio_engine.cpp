#include "veloz/engine/stdio_engine.h"
#include "veloz/core/logger.h"
#include <ostream>
#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>

namespace veloz {
namespace engine {

StdioEngine::StdioEngine(std::ostream& out)
    : out_(out), command_count_(0) {
}

void StdioEngine::emit_event(const std::string& event_json) {
    std::lock_guard<std::mutex> lock(output_mutex_);
    out_ << event_json << std::endl;
    out_.flush();
}

void StdioEngine::emit_error(const std::string& error_msg) {
    std::ostringstream oss;
    oss << R"({
        "type": "error",
        "message": ")" << error_msg << R"("
    })";
    emit_event(oss.str());
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
    })");

    while (!stop_flag.load() && std::getline(std::cin, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        command_count_++;

        // Parse the command
        ParsedCommand command = parse_command(line);

        switch (command.type) {
            case CommandType::Order:
                if (command.order) {
                    // Emit order received event
                    std::ostringstream oss;
                    oss << R"({
                        "type": "order_received",
                        "command_id": )" << command_count_ << R"(,
                        "client_order_id": ")" << command.order->request.client_order_id << R"(",
                        "symbol": ")" << command.order->request.symbol.value << R"(",
                        "side": ")" << (command.order->request.side == veloz::exec::OrderSide::Buy ? "buy" : "sell") << R"(",
                        "type": ")" << (command.order->request.type == veloz::exec::OrderType::Market ? "market" : "limit") << R"(",
                        "quantity": )" << command.order->request.qty << R"(,
                        "price": )" << command.order->request.price.value_or(0.0) << R"(
                    })";
                    emit_event(oss.str());

                    // Call handler if registered
                    if (order_handler_) {
                        order_handler_(*command.order);
                    }
                } else {
                    emit_error("Failed to parse ORDER command: " + command.error);
                }
                break;

            case CommandType::Cancel:
                if (command.cancel) {
                    // Emit cancel received event
                    std::ostringstream oss;
                    oss << R"({
                        "type": "cancel_received",
                        "command_id": )" << command_count_ << R"(,
                        "client_order_id": ")" << command.cancel->client_order_id << R"("
                    })";
                    emit_event(oss.str());

                    // Call handler if registered
                    if (cancel_handler_) {
                        cancel_handler_(*command.cancel);
                    }
                } else {
                    emit_error("Failed to parse CANCEL command: " + command.error);
                }
                break;

            case CommandType::Query:
                if (command.query) {
                    // Emit query received event
                    std::ostringstream oss;
                    oss << R"({
                        "type": "query_received",
                        "command_id": )" << command_count_ << R"(,
                        "query_type": ")" << command.query->query_type << R"(",
                        "params": ")" << command.query->params << R"("
                    })";
                    emit_event(oss.str());

                    // Call handler if registered
                    if (query_handler_) {
                        query_handler_(*command.query);
                    }
                } else {
                    emit_error("Failed to parse QUERY command: " + command.error);
                }
                break;

            case CommandType::Unknown:
                if (!command.error.empty()) {
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
        "commands_processed": )" << command_count_ << R"(
    })";
        emit_event(oss.str());
    }

    return 0;
}

} // namespace engine
} // namespace veloz
