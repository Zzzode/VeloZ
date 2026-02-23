#include "veloz/engine/stdio_engine.h"

#include "veloz/core/logger.h"
#include "veloz/strategy/advanced_strategies.h"
#include "veloz/strategy/mean_reversion_strategy.h"
#include "veloz/strategy/trend_following_strategy.h"

#include <cstdlib>
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/io.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz {
namespace engine {

namespace {
// Helper to extract value from kj::Maybe with a default
template <typename T> T maybe_or(const kj::Maybe<T>& maybe, T default_value) {
  T result = default_value;
  KJ_IF_SOME(val, maybe) {
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
    : out_(out), in_(in), output_mutex_(0), command_count_(0),
      strategy_manager_(kj::heap<veloz::strategy::StrategyManager>()) {
  // Register built-in strategy factories
  strategy_manager_->register_strategy_factory(kj::rc<strategy::TrendFollowingStrategyFactory>());
  strategy_manager_->register_strategy_factory(kj::rc<strategy::MeanReversionStrategyFactory>());
  strategy_manager_->register_strategy_factory(kj::rc<strategy::RsiStrategyFactory>());
  strategy_manager_->register_strategy_factory(kj::rc<strategy::MacdStrategyFactory>());
  strategy_manager_->register_strategy_factory(kj::rc<strategy::BollingerBandsStrategyFactory>());
  strategy_manager_->register_strategy_factory(
      kj::rc<strategy::StochasticOscillatorStrategyFactory>());
  strategy_manager_->register_strategy_factory(kj::rc<strategy::MarketMakingHFTStrategyFactory>());
  strategy_manager_->register_strategy_factory(
      kj::rc<strategy::CrossExchangeArbitrageStrategyFactory>());
}

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
  kj::String banner = kj::str("VeloZ StdioEngine started - press Ctrl+C to stop\n",
                              "Commands: ORDER <SIDE> <SYMBOL> <QTY> <PRICE> <ID> [TYPE] [TIF]\n",
                              "          CANCEL <ID>\n", "          QUERY <TYPE> [PARAMS]\n",
                              "          STRATEGY LOAD <TYPE> <NAME> [PARAMS...]\n",
                              "          STRATEGY START|STOP|PAUSE|RESUME|UNLOAD <STRATEGY_ID>\n",
                              "          STRATEGY LIST|STATUS [STRATEGY_ID]\n",
                              "          STRATEGY PARAMS <STRATEGY_ID> <KEY>=<VALUE>...\n",
                              "          STRATEGY METRICS [STRATEGY_ID]\n\n");
  out_.write(banner.asBytes());

  // Send startup event
  emit_event(R"({"type":"engine_started","version":"1.0.0"})"_kj);

  while (!*stop_flag.lockShared()) {
    auto maybe_line = read_line(in_);
    KJ_IF_SOME(line, maybe_line) {
      // Skip empty lines and comments
      if (line.size() == 0 || line[0] == '#') {
        continue;
      }

      command_count_++;

      // Parse the command
      ParsedCommand command = parse_command(line);

      switch (command.type) {
      case CommandType::Order:
        KJ_IF_SOME(order, command.order) {
          // Emit order received event
          kj::String json = kj::str(
              R"({"type":"order_received","command_id":)", command_count_,
              R"(,"client_order_id":")", order.request.client_order_id, R"(","symbol":")",
              order.request.symbol.value, R"(","side":")",
              (order.request.side == veloz::exec::OrderSide::Buy ? "buy" : "sell"),
              R"(","order_type":")",
              (order.request.type == veloz::exec::OrderType::Market ? "market" : "limit"),
              R"(","quantity":)", order.request.qty, R"(,"price":)",
              maybe_or(order.request.price, 0.0), "}");
          emit_event(json);

          // Call handler if registered
          KJ_IF_SOME(handler, order_handler_) {
            handler(order);
          }
        }
        else {
          emit_error(kj::str("Failed to parse ORDER command: ", command.error));
        }
        break;

      case CommandType::Cancel:
        KJ_IF_SOME(cancel, command.cancel) {
          // Emit cancel received event
          kj::String json = kj::str(R"({"type":"cancel_received","command_id":)", command_count_,
                                    R"(,"client_order_id":")", cancel.client_order_id, R"("})");
          emit_event(json);

          // Call handler if registered
          KJ_IF_SOME(handler, cancel_handler_) {
            handler(cancel);
          }
        }
        else {
          emit_error(kj::str("Failed to parse CANCEL command: ", command.error));
        }
        break;

      case CommandType::Query:
        KJ_IF_SOME(query, command.query) {
          // Emit query received event
          kj::String json = kj::str(R"({"type":"query_received","command_id":)", command_count_,
                                    R"(,"query_type":")", query.query_type, R"(","params":")",
                                    query.params, R"("})");
          emit_event(json);

          // Call handler if registered
          KJ_IF_SOME(handler, query_handler_) {
            handler(query);
          }
        }
        else {
          emit_error(kj::str("Failed to parse QUERY command: ", command.error));
        }
        break;

      case CommandType::Strategy:
        KJ_IF_SOME(strategy_cmd, command.strategy) {
          // Emit strategy command received event
          kj::String json =
              kj::str(R"({"type":"strategy_command_received","command_id":)", command_count_,
                      R"(,"subcommand":")", static_cast<int>(strategy_cmd.subcommand), R"("})");
          emit_event(json);

          // Handle strategy command internally
          handle_strategy_command(strategy_cmd);

          // Call handler if registered
          KJ_IF_SOME(handler, strategy_handler_) {
            handler(strategy_cmd);
          }
        }
        else {
          emit_error(kj::str("Failed to parse STRATEGY command: ", command.error));
        }
        break;

      case CommandType::Unknown:
        if (command.error.size() > 0) {
          emit_error(command.error);
        }
        break;

      default:
        // Handle any other command types (Subscribe, Unsubscribe, etc.)
        emit_error(kj::str("Unhandled command type"));
        break;
      }
    }
    else {
      // EOF reached
      break;
    }
  }

  // Send shutdown event
  kj::String shutdown_json =
      kj::str(R"({"type":"engine_stopped","commands_processed":)", command_count_, "}");
  emit_event(shutdown_json);

  return 0;
}

void StdioEngine::handle_strategy_command(const ParsedStrategy& cmd) {
  // Error isolation: wrap all strategy operations in exception handling
  // to prevent strategy errors from crashing the engine
  try {
    switch (cmd.subcommand) {
    case StrategySubCommand::Load: {
      // Create strategy config from command
      strategy::StrategyConfig config;
      config.name = kj::str(cmd.strategy_name);
      config.risk_per_trade = 0.02;
      config.max_position_size = 1.0;
      config.stop_loss = 0.05;
      config.take_profit = 0.1;
      config.symbols.add(kj::str("BTCUSDT"));

      // Map type string to StrategyType enum
      kj::String type_lower = kj::str(cmd.strategy_type);
      // Convert to lowercase for comparison
      for (size_t i = 0; i < type_lower.size(); ++i) {
        if (type_lower[i] >= 'A' && type_lower[i] <= 'Z') {
          const_cast<char*>(type_lower.begin())[i] = type_lower[i] + 32;
        }
      }

      if (type_lower == "rsistrategy"_kj || type_lower == "rsi"_kj) {
        config.type = strategy::StrategyType::Custom;
      } else if (type_lower == "macdstrategy"_kj || type_lower == "macd"_kj) {
        config.type = strategy::StrategyType::Custom;
      } else if (type_lower == "bollingerbandsstrategy"_kj || type_lower == "bollinger"_kj) {
        config.type = strategy::StrategyType::Custom;
      } else if (type_lower == "stochasticoscillatorstrategy"_kj || type_lower == "stochastic"_kj) {
        config.type = strategy::StrategyType::Custom;
      } else if (type_lower == "marketmakinghftstrategy"_kj || type_lower == "marketmaking"_kj ||
                 type_lower == "mm"_kj) {
        config.type = strategy::StrategyType::MarketMaking;
      } else if (type_lower == "crossexchangearbitragestrategy"_kj ||
                 type_lower == "arbitrage"_kj) {
        config.type = strategy::StrategyType::Arbitrage;
      } else if (type_lower == "trendfollowing"_kj || type_lower == "trend"_kj) {
        config.type = strategy::StrategyType::TrendFollowing;
      } else if (type_lower == "meanreversion"_kj || type_lower == "reversion"_kj) {
        config.type = strategy::StrategyType::MeanReversion;
      } else if (type_lower == "momentum"_kj) {
        config.type = strategy::StrategyType::Momentum;
      } else if (type_lower == "grid"_kj) {
        config.type = strategy::StrategyType::Grid;
      } else {
        config.type = strategy::StrategyType::Custom;
      }

      // Create a logger for the strategy
      auto console_output = kj::heap<veloz::core::ConsoleOutput>(true);
      veloz::core::Logger logger(kj::heap<veloz::core::TextFormatter>(), kj::mv(console_output));

      kj::String strategy_id = strategy_manager_->load_strategy(config, logger);
      if (strategy_id.size() > 0) {
        kj::String json =
            kj::str(R"({"type":"strategy_loaded","strategy_id":")", strategy_id, R"(","name":")",
                    cmd.strategy_name, R"(","strategy_type":")", cmd.strategy_type, R"("})");
        emit_event(json);
      } else {
        emit_error(kj::str("Failed to load strategy: ", cmd.strategy_name));
      }
      break;
    }

    case StrategySubCommand::Start: {
      bool success = strategy_manager_->start_strategy(cmd.strategy_id);
      if (success) {
        kj::String json =
            kj::str(R"({"type":"strategy_started","strategy_id":")", cmd.strategy_id, R"("})");
        emit_event(json);
      } else {
        emit_error(kj::str("Failed to start strategy: ", cmd.strategy_id));
      }
      break;
    }

    case StrategySubCommand::Stop: {
      bool success = strategy_manager_->stop_strategy(cmd.strategy_id);
      if (success) {
        kj::String json =
            kj::str(R"({"type":"strategy_stopped","strategy_id":")", cmd.strategy_id, R"("})");
        emit_event(json);
      } else {
        emit_error(kj::str("Failed to stop strategy: ", cmd.strategy_id));
      }
      break;
    }

    case StrategySubCommand::Pause: {
      bool success = strategy_manager_->pause_strategy(cmd.strategy_id);
      if (success) {
        kj::String json =
            kj::str(R"({"type":"strategy_paused","strategy_id":")", cmd.strategy_id, R"("})");
        emit_event(json);
      } else {
        emit_error(kj::str("Failed to pause strategy: ", cmd.strategy_id));
      }
      break;
    }

    case StrategySubCommand::Resume: {
      bool success = strategy_manager_->resume_strategy(cmd.strategy_id);
      if (success) {
        kj::String json =
            kj::str(R"({"type":"strategy_resumed","strategy_id":")", cmd.strategy_id, R"("})");
        emit_event(json);
      } else {
        emit_error(kj::str("Failed to resume strategy: ", cmd.strategy_id));
      }
      break;
    }

    case StrategySubCommand::Unload: {
      bool success = strategy_manager_->unload_strategy(cmd.strategy_id);
      if (success) {
        kj::String json =
            kj::str(R"({"type":"strategy_unloaded","strategy_id":")", cmd.strategy_id, R"("})");
        emit_event(json);
      } else {
        emit_error(kj::str("Failed to unload strategy: ", cmd.strategy_id));
      }
      break;
    }

    case StrategySubCommand::List: {
      auto ids = strategy_manager_->get_all_strategy_ids();
      kj::String ids_json = kj::str("[");
      for (size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) {
          ids_json = kj::str(ids_json, ",");
        }
        ids_json = kj::str(ids_json, "\"", ids[i], "\"");
      }
      ids_json = kj::str(ids_json, "]");

      kj::String json = kj::str(R"({"type":"strategy_list","count":)", ids.size(),
                                R"(,"strategies":)", ids_json, "}");
      emit_event(json);
      break;
    }

    case StrategySubCommand::Status: {
      if (cmd.strategy_id.size() > 0) {
        // Get status for specific strategy
        auto strategy = strategy_manager_->get_strategy(cmd.strategy_id);
        if (strategy != nullptr) {
          auto state = strategy->get_state();
          kj::String json = kj::str(R"({"type":"strategy_status","strategy_id":")",
                                    state.strategy_id, R"(","name":")", state.strategy_name,
                                    R"(","is_running":)", (state.is_running ? "true" : "false"),
                                    R"(,"pnl":)", state.pnl, R"(,"trade_count":)",
                                    state.trade_count, R"(,"win_rate":)", state.win_rate, "}");
          emit_event(json);
        } else {
          emit_error(kj::str("Strategy not found: ", cmd.strategy_id));
        }
      } else {
        // Get status for all strategies
        auto states = strategy_manager_->get_all_strategy_states();
        kj::String json = kj::str(R"({"type":"strategy_status_all","count":)", states.size(),
                                  R"(,"strategies":[)");
        for (size_t i = 0; i < states.size(); ++i) {
          if (i > 0) {
            json = kj::str(json, ",");
          }
          json = kj::str(json, R"({"strategy_id":")", states[i].strategy_id, R"(","name":")",
                         states[i].strategy_name, R"(","is_running":)",
                         (states[i].is_running ? "true" : "false"), R"(,"pnl":)", states[i].pnl,
                         R"(,"trade_count":)", states[i].trade_count, "}");
        }
        json = kj::str(json, "]}");
        emit_event(json);
      }
      break;
    }

    case StrategySubCommand::Params: {
      // Parse key=value pairs from params string
      kj::TreeMap<kj::String, double> parameters;

      // Simple parser for "key1=value1 key2=value2" format
      kj::StringPtr params_str = cmd.params;
      size_t i = 0;
      while (i < params_str.size()) {
        // Skip whitespace
        while (i < params_str.size() && (params_str[i] == ' ' || params_str[i] == '\t')) {
          ++i;
        }
        if (i >= params_str.size()) {
          break;
        }

        // Find key
        size_t key_start = i;
        while (i < params_str.size() && params_str[i] != '=') {
          ++i;
        }
        if (i >= params_str.size()) {
          break;
        }
        kj::String key = kj::str(params_str.slice(key_start, i));
        ++i; // Skip '='

        // Find value
        size_t value_start = i;
        while (i < params_str.size() && params_str[i] != ' ' && params_str[i] != '\t') {
          ++i;
        }
        kj::String value_str = kj::str(params_str.slice(value_start, i));
        double value = std::strtod(value_str.cStr(), nullptr);

        parameters.insert(kj::mv(key), value);
      }

      bool success = strategy_manager_->reload_parameters(cmd.strategy_id, parameters);
      if (success) {
        kj::String json = kj::str(R"({"type":"strategy_params_updated","strategy_id":")",
                                  cmd.strategy_id, R"(","param_count":)", parameters.size(), "}");
        emit_event(json);
      } else {
        emit_error(kj::str("Failed to update parameters for strategy: ", cmd.strategy_id));
      }
      break;
    }

    case StrategySubCommand::Metrics: {
      if (cmd.strategy_id.size() > 0) {
        // Get metrics for specific strategy
        auto maybe_metrics = strategy_manager_->get_strategy_metrics(cmd.strategy_id);
        KJ_IF_SOME(metrics, maybe_metrics) {
          kj::String json = kj::str(
              R"({"type":"strategy_metrics","strategy_id":")", cmd.strategy_id,
              R"(","events_processed":)", metrics.events_processed.load(),
              R"(,"signals_generated":)", metrics.signals_generated.load(),
              R"(,"avg_execution_time_us":)", metrics.avg_execution_time_us(),
              R"(,"signals_per_second":)", metrics.signals_per_second(), R"(,"errors":)",
              metrics.errors.load(), "}");
          emit_event(json);
        }
        else {
          emit_error(kj::str("No metrics available for strategy: ", cmd.strategy_id));
        }
      } else {
        // Get aggregated metrics summary
        kj::String summary = strategy_manager_->get_metrics_summary();
        kj::String json = kj::str(R"({"type":"strategy_metrics_summary","summary":")",
                                  summary.cStr(), R"("})");
        emit_event(json);
      }
      break;
    }

    case StrategySubCommand::Unknown:
    default:
      emit_error(kj::str("Unknown strategy subcommand"));
      break;
    }
  } catch (const kj::Exception& e) {
    // KJ exception - strategy operation failed
    emit_error(kj::str("Strategy error (KJ): ", e.getDescription()));
  } catch (const std::exception& e) {
    // Standard exception - strategy operation failed
    emit_error(kj::str("Strategy error: ", e.what()));
  } catch (...) {
    // Unknown exception - strategy operation failed
    emit_error(kj::str("Strategy error: unknown exception"));
  }
}

} // namespace engine
} // namespace veloz
