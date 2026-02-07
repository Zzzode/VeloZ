#include "veloz/market/order_book.h"

namespace veloz::market {

void OrderBook::apply_snapshot(const std::vector<BookLevel>& bids,
                                const std::vector<BookLevel>& asks,
                                std::int64_t sequence) {
    bids_.clear();
    asks_.clear();

    for (const auto& level : bids) {
        if (level.qty > 0.0) {
            bids_[level.price] = level.qty;
        }
    }

    for (const auto& level : asks) {
        if (level.qty > 0.0) {
            asks_[level.price] = level.qty;
        }
    }

    sequence_ = sequence;
    rebuild_cache();
}

void OrderBook::apply_delta(const BookLevel& level, bool is_bid, std::int64_t sequence) {
    // Reject out-of-order updates
    if (sequence <= sequence_) {
        return;
    }

    if (is_bid) {
        if (level.qty == 0.0) {
            bids_.erase(level.price);  // delete
        } else {
            bids_[level.price] = level.qty;  // update/insert
        }
    } else {
        if (level.qty == 0.0) {
            asks_.erase(level.price);  // delete
        } else {
            asks_[level.price] = level.qty;  // update/insert
        }
    }

    sequence_ = sequence;
    rebuild_cache();
}

const std::vector<BookLevel>& OrderBook::bids() const {
    return bids_cache_;
}

const std::vector<BookLevel>& OrderBook::asks() const {
    return asks_cache_;
}

std::optional<BookLevel> OrderBook::best_bid() const {
    if (bids_cache_.empty()) return std::nullopt;
    return bids_cache_.front();
}

std::optional<BookLevel> OrderBook::best_ask() const {
    if (asks_cache_.empty()) return std::nullopt;
    return asks_cache_.front();
}

double OrderBook::spread() const {
    auto bid = best_bid();
    auto ask = best_ask();
    if (!bid || !ask) return 0.0;
    return ask->price - bid->price;
}

double OrderBook::mid_price() const {
    auto bid = best_bid();
    auto ask = best_ask();
    if (!bid || !ask) return 0.0;
    return (bid->price + ask->price) / 2.0;
}

std::int64_t OrderBook::sequence() const {
    return sequence_;
}

std::vector<BookLevel> OrderBook::top_bids(std::size_t n) const {
    if (n >= bids_cache_.size()) return bids_cache_;
    return std::vector<BookLevel>(bids_cache_.begin(), bids_cache_.begin() + n);
}

std::vector<BookLevel> OrderBook::top_asks(std::size_t n) const {
    if (n >= asks_cache_.size()) return asks_cache_;
    return std::vector<BookLevel>(asks_cache_.begin(), asks_cache_.begin() + n);
}

void OrderBook::rebuild_cache() {
    bids_cache_.clear();
    bids_cache_.reserve(bids_.size());
    for (const auto& [price, qty] : bids_) {
        bids_cache_.push_back({price, qty});
    }

    asks_cache_.clear();
    asks_cache_.reserve(asks_.size());
    for (const auto& [price, qty] : asks_) {
        asks_cache_.push_back({price, qty});
    }
}

} // namespace veloz::market
