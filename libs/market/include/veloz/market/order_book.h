#pragma once

#include "veloz/market/market_event.h"

#include <map>
#include <optional>
#include <vector>

namespace veloz::market {

class OrderBook final {
public:
    OrderBook() = default;

    // Apply full snapshot (replaces existing book)
    void apply_snapshot(const std::vector<BookLevel>& bids,
                        const std::vector<BookLevel>& asks,
                        std::int64_t sequence);

    // Apply incremental delta (update/delete a level)
    void apply_delta(const BookLevel& level, bool is_bid, std::int64_t sequence);

    // Query methods
    [[nodiscard]] const std::vector<BookLevel>& bids() const;
    [[nodiscard]] const std::vector<BookLevel>& asks() const;
    [[nodiscard]] std::optional<BookLevel> best_bid() const;
    [[nodiscard]] std::optional<BookLevel> best_ask() const;
    [[nodiscard]] double spread() const;
    [[nodiscard]] double mid_price() const;
    [[nodiscard]] std::int64_t sequence() const;

    // Get top N levels
    [[nodiscard]] std::vector<BookLevel> top_bids(std::size_t n) const;
    [[nodiscard]] std::vector<BookLevel> top_asks(std::size_t n) const;

private:
    void rebuild_cache();

    std::map<double, double, std::greater<double>> bids_; // descending
    std::map<double, double> asks_;                       // ascending

    // Cache for fast queries (bids[0] = best bid, asks[0] = best ask)
    std::vector<BookLevel> bids_cache_;
    std::vector<BookLevel> asks_cache_;

    std::int64_t sequence_{0};
};

} // namespace veloz::market
