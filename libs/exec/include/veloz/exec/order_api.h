#pragma once

#include "veloz/common/types.h"

#include <cstdint>
#include <optional>
#include <string>

namespace veloz::exec {

enum class OrderSide : std::uint8_t { Buy = 0, Sell = 1 };
enum class OrderType : std::uint8_t { Market = 0, Limit = 1 };
enum class TimeInForce : std::uint8_t { GTC = 0, IOC = 1, FOK = 2, GTX = 3 };

struct PlaceOrderRequest final {
  veloz::common::SymbolId symbol;
  OrderSide side{OrderSide::Buy};
  OrderType type{OrderType::Limit};
  TimeInForce tif{TimeInForce::GTC};

  double qty{0.0};
  std::optional<double> price;

  std::string client_order_id;
  bool reduce_only{false};
  bool post_only{false};
};

struct CancelOrderRequest final {
  veloz::common::SymbolId symbol;
  std::string client_order_id;
};

enum class OrderStatus : std::uint8_t {
  New = 0,
  Accepted = 1,
  PartiallyFilled = 2,
  Filled = 3,
  Canceled = 4,
  Rejected = 5,
  Expired = 6,
};

struct ExecutionReport final {
  veloz::common::SymbolId symbol;
  std::string client_order_id;
  std::string venue_order_id;
  OrderStatus status{OrderStatus::New};

  double last_fill_qty{0.0};
  double last_fill_price{0.0};

  std::int64_t ts_exchange_ns{0};
  std::int64_t ts_recv_ns{0};
};

} // namespace veloz::exec
