#include "veloz/engine/stdio_engine.h"
#include "veloz/engine/event_emitter.h"
#include "veloz/exec/order_router.h"
#include "veloz/risk/risk_engine.h"
#include "veloz/market/order_book.h"
#include "veloz/market/subscription_manager.h"
#include "veloz/exec/binance_adapter.h"
#include "veloz/exec/order_api.h"
#include "veloz/core/time.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <iostream>
#include <chrono>

namespace veloz::engine {

StdioEngine::StdioEngine(std::ostream& out)
    : emitter_(out) {
    order_router_ = std::make_shared<veloz::exec::OrderRouter>();
    risk_engine_ = std::make_shared<veloz::risk::RiskEngine>();
    subscription_manager_ = std::make_shared<veloz::market::SubscriptionManager>();

    // Initialize Binance adapter with empty keys (placeholder for testing)
    auto binance_adapter = std::make_shared<veloz::exec::BinanceAdapter>("", "", false);

    // Register Binance as default venue
    order_router_->set_default_venue(veloz::common::Venue::Binance);
    order_router_->register_adapter(veloz::common::Venue::Binance, binance_adapter);
}

int StdioEngine::run(std::atomic<bool>& stop_flag) {
    state_.init_balances();
    emitter_.emit_account(veloz::core::now_unix_ns(), state_.snapshot_balances());

    // Command: SET_BINANCE_KEY <api_key>
    // Command: BINANCE_CONNECT - Connect to Binance
    // Command: BINANCE_DISCONNECT - Disconnect from Binance
    std::unordered_map<std::string, std::function<void(const std::string&)>> commands = {
        {"SET_BINANCE_KEY", [&](const std::string& /* unused args */) {
            // This would normally configure the adapter
            // For now, just acknowledge
            std::cout << "SET_BINANCE_KEY received (not implemented in this demo)" << std::endl;
        }},
        {"BINANCE_CONNECT", [&](const std::string& /* unused args */) {
            auto adapter = order_router_->get_adapter(veloz::common::Venue::Binance);
            if (adapter) {
                adapter->connect();
                std::cout << "Connecting to Binance..." << std::endl;
            } else {
                std::cout << "No Binance adapter registered" << std::endl;
            }
        }},
        {"BINANCE_DISCONNECT", [&](const std::string& /* unused args */) {
            auto adapter = order_router_->get_adapter(veloz::common::Venue::Binance);
            if (adapter) {
                adapter->disconnect();
                std::cout << "Disconnected from Binance" << std::endl;
            }
        }},
    };

    std::jthread market_thread([&] {
        using namespace std::chrono_literals;
        std::uint64_t tick = 0;
        while (!stop_flag.load()) {
            tick++;
            const double drift = (static_cast<double>((tick % 19)) - 9.0) * 0.25;
            const double noise = (static_cast<double>((tick % 7)) - 3.0) * 0.10;
            double next = state_.price() + drift + noise;
            next = clamp(next, 1000.0, 100000.0);
            state_.set_price(next);
            const auto ts = veloz::core::now_unix_ns();
            emitter_.emit_market("BTCUSDT", next, ts);
            std::this_thread::sleep_for(200ms);
        }
    });

    std::jthread fill_thread([&] {
        using namespace std::chrono_literals;
        while (!stop_flag.load()) {
            const auto now = veloz::core::now_unix_ns();
            auto due = state_.collect_due_fills(now);
            for (const auto& po : due) {
                const auto ts = veloz::core::now_unix_ns();
                const double fill_price = state_.price();
                emitter_.emit_fill(po.request.client_order_id, po.request.symbol.value, po.request.qty,
                                   fill_price, ts);
                state_.apply_fill(po, fill_price, ts);
                if (const auto st = state_.get_order_state(po.request.client_order_id); st.has_value()) {
                    emitter_.emit_order_state(st.value());
                }
                emitter_.emit_account(ts, state_.snapshot_balances());
            }
            std::this_thread::sleep_for(20ms);
        }
    });

    std::string line;
    while (!stop_flag.load() && std::getline(std::cin, line)) {
        const auto parsed = parse_order_command(line);
        if (parsed.has_value()) {
            const auto ts = veloz::core::now_unix_ns();

            // Risk check
            const auto risk_result = risk_engine_->check_pre_trade(parsed->request);
            if (!risk_result.allowed) {
                emitter_.emit_order_update(parsed->request.client_order_id, "REJECTED",
                                        parsed->request.symbol.value,
                                        side_to_string(parsed->request.side), parsed->request.qty,
                                        parsed->request.price.value_or(0.0), "", risk_result.reason, ts);
                if (const auto st = state_.get_order_state(parsed->request.client_order_id); st.has_value()) {
                    emitter_.emit_order_state(st.value());
                }
                continue;
            }

            const auto decision = state_.place_order(parsed->request, ts);
            if (!decision.accepted) {
                emitter_.emit_order_update(parsed->request.client_order_id, "REJECTED",
                                        parsed->request.symbol.value,
                                        side_to_string(parsed->request.side), parsed->request.qty,
                                        parsed->request.price.value_or(0.0), "", decision.reason, ts);
                if (const auto st = state_.get_order_state(parsed->request.client_order_id); st.has_value()) {
                    emitter_.emit_order_state(st.value());
                }
                continue;
            }

            // Route order via OrderRouter
            const auto execution_report = order_router_->place_order(
                parsed->request.venue.value_or(veloz::common::Venue::Binance),
                parsed->request);

            if (execution_report.has_value()) {
                emitter_.emit_order_update(parsed->request.client_order_id, "ACCEPTED",
                                        parsed->request.symbol.value,
                                        side_to_string(parsed->request.side), parsed->request.qty,
                                        parsed->request.price.value_or(0.0),
                                        execution_report->venue_order_id, "", ts);
            } else {
                emitter_.emit_order_update(parsed->request.client_order_id, "REJECTED",
                                        parsed->request.symbol.value,
                                        side_to_string(parsed->request.side), parsed->request.qty,
                                        parsed->request.price.value_or(0.0), "",
                                        "No execution report from router", ts);
            }

            if (const auto st = state_.get_order_state(parsed->request.client_order_id); st.has_value()) {
                emitter_.emit_order_state(st.value());
            }
            emitter_.emit_account(ts, state_.snapshot_balances());
            continue;
        }

        const auto cancel = parse_cancel_command(line);
        if (cancel.has_value()) {
            const auto ts = veloz::core::now_unix_ns();
            const auto decision = state_.cancel_order(cancel->client_order_id, ts);
            if (decision.found && decision.cancelled.has_value()) {
                const auto& po = decision.cancelled.value();

                // Risk check before cancel
                const auto risk_result = risk_engine_->check_post_trade(po.position);
                if (!risk_result.allowed) {
                    emitter_.emit_order_update(cancel->client_order_id, "REJECTED", "", "",
                                            std::nullopt, std::nullopt, risk_result.reason, ts);
                    if (const auto st = state_.get_order_state(cancel->client_order_id); st.has_value()) {
                        emitter_.emit_order_state(st.value());
                    }
                    continue;
                }

                // Route cancel via OrderRouter
                const auto execution_report = order_router_->cancel_order(
                    po.request.venue.value_or(veloz::common::Venue::Binance),
                    cancel->request);

                if (execution_report.has_value()) {
                    emitter_.emit_order_update(po.request.client_order_id, "CANCELED",
                                            po.request.symbol.value, side_to_string(po.request.side),
                                            po.request.qty, po.request.price.value_or(0.0), "", "", ts);
                } else {
                    emitter_.emit_order_update(cancel->client_order_id, "REJECTED", "", "",
                                            std::nullopt, std::nullopt,
                                            "No execution report from router", ts);
                }

                if (const auto st = state_.get_order_state(cancel->client_order_id); st.has_value()) {
                    emitter_.emit_order_state(st.value());
                }
                emitter_.emit_account(ts, state_.snapshot_balances());
            } else {
                emitter_.emit_order_update(cancel->client_order_id, "REJECTED", "", std::nullopt,
                                            std::nullopt, "", decision.reason, ts);
                if (const auto st = state_.get_order_state(cancel->client_order_id); st.has_value()) {
                    emitter_.emit_order_state(st.value());
                }
            }
            continue;
        }

        // Check for Binance management commands
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        auto it = commands.find(cmd);
        if (it != commands.end()) {
            std::string args;
            std::getline(iss, args);
            it->second(args);
        } else {
            emitter_.emit_error("unknown_command", veloz::core::now_unix_ns());
        }
    }

    emitter_.emit_error("shutdown_requested", veloz::core::now_unix_ns());
    stop_flag.store(true);
    return 0;
}

void StdioEngine::handle_market_event(const veloz::market::MarketEvent& event) {
    std::lock_guard<std::mutex> lock(market_mu_);

    // Update order book if BookTop or BookDelta event
    if (event.type == veloz::market::MarketEventType::BookTop) {
        auto it = order_books_.find(event.symbol.value);
        if (it == order_books_.end()) {
            order_books_[event.symbol.value] = veloz::market::OrderBook();
            it = order_books_.find(event.symbol.value);
        }

        if (std::holds_alternative<veloz::market::BookData>(event.data)) {
            const auto& book_data = std::get<veloz::market::BookData>(event.data);
            it->second.apply_snapshot(book_data.bids, book_data.asks, book_data.sequence);
        }
    } else if (event.type == veloz::market::MarketEventType::BookDelta) {
        auto it = order_books_.find(event.symbol.value);
        if (it != order_books_.end() && std::holds_alternative<veloz::market::BookData>(event.data)) {
            const auto& book_data = std::get<veloz::market::BookData>(event.data);
            for (const auto& bid : book_data.bids) {
                it->second.apply_delta(bid, true, book_data.sequence);
            }
            for (const auto& ask : book_data.asks) {
                it->second.apply_delta(ask, false, book_data.sequence);
            }
        }
    } else if (event.type == veloz::market::MarketEventType::Trade) {
        // Handle trade events
        // Could trigger fills based on our orders
    }
}
