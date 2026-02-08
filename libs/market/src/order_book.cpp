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

double OrderBook::depth_at_price(double price, bool is_bid) const {
    if (is_bid) {
        auto it = bids_.find(price);
        return (it != bids_.end()) ? it->second : 0.0;
    } else {
        auto it = asks_.find(price);
        return (it != asks_.end()) ? it->second : 0.0;
    }
}

double OrderBook::total_depth(bool is_bid) const {
    double total = 0.0;
    if (is_bid) {
        for (const auto& [_, qty] : bids_) {
            total += qty;
        }
    } else {
        for (const auto& [_, qty] : asks_) {
            total += qty;
        }
    }
    return total;
}

double OrderBook::cumulative_depth(double price, bool is_bid) const {
    double cumulative = 0.0;

    if (is_bid) {
        for (const auto& [level_price, qty] : bids_) {
            if (level_price >= price) {
                cumulative += qty;
            } else {
                break;
            }
        }
    } else {
        for (const auto& [level_price, qty] : asks_) {
            if (level_price <= price) {
                cumulative += qty;
            } else {
                break;
            }
        }
    }

    return cumulative;
}

std::vector<std::pair<double, double>> OrderBook::liquidity_profile(
    bool is_bid, double price_range, double step) const {

    std::vector<std::pair<double, double>> profile;
    auto ref_price = is_bid ? best_bid() : best_ask();

    if (!ref_price) {
        return profile;
    }

    double start_price = is_bid ? (ref_price->price - price_range) : ref_price->price;
    double end_price = is_bid ? ref_price->price : (ref_price->price + price_range);

    for (double price = start_price; price <= end_price; price += step) {
        double depth = cumulative_depth(price, is_bid);
        profile.emplace_back(price, depth);
    }

    return profile;
}

double OrderBook::market_impact(double qty, bool is_bid) const {
    double cumulative_qty = 0.0;
    double avg_price = 0.0;

    const auto& cache = is_bid ? bids_cache_ : asks_cache_;

    for (const auto& level : cache) {
        if (cumulative_qty >= qty) {
            break;
        }

        double available_qty = level.qty;
        if (cumulative_qty + available_qty > qty) {
            available_qty = qty - cumulative_qty;
        }

        avg_price = (avg_price * cumulative_qty + level.price * available_qty) /
                  (cumulative_qty + available_qty);
        cumulative_qty += available_qty;
    }

    if (cumulative_qty < qty) {
        return 0.0; // Not enough liquidity
    }

    return avg_price;
}

double OrderBook::volume_weighted_average_price(bool is_bid, double depth) const {
    double total_qty = 0.0;
    double total_notional = 0.0;

    const auto& cache = is_bid ? bids_cache_ : asks_cache_;

    for (const auto& level : cache) {
        if (total_qty >= depth) {
            break;
        }

        double available_qty = level.qty;
        if (total_qty + available_qty > depth) {
            available_qty = depth - total_qty;
        }

        total_qty += available_qty;
        total_notional += level.price * available_qty;
    }

    if (total_qty == 0.0) {
        return 0.0;
    }

    return total_notional / total_qty;
}

std::size_t OrderBook::level_count(bool is_bid) const {
    return is_bid ? bids_.size() : asks_.size();
}

double OrderBook::average_level_size(bool is_bid) const {
    double total_depth = this->total_depth(is_bid);
    std::size_t levels = level_count(is_bid);

    if (levels == 0) {
        return 0.0;
    }

    return total_depth / static_cast<double>(levels);
}

void OrderBook::clear() {
    bids_.clear();
    asks_.clear();
    bids_cache_.clear();
    asks_cache_.clear();
    sequence_ = 0;
}

bool OrderBook::empty() const {
    return bids_.empty() && asks_.empty();
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
