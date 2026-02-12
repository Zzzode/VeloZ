#include "veloz/exec/binance_adapter.h"
#include "veloz/core/json.h"

#ifndef VELOZ_NO_CURL
#include <curl/curl.h>
#endif

#ifndef VELOZ_NO_OPENSSL
#include <openssl/hmac.h>
#include <openssl/evp.h>
#endif

#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <vector>

namespace veloz::exec {

using namespace veloz::core;

#ifndef VELOZ_NO_CURL
// Static helper for CURL
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}
#endif

BinanceAdapter::BinanceAdapter(const std::string& api_key, const std::string& secret_key, bool testnet)
    : api_key_(api_key)
    , secret_key_(secret_key)
    , connected_(false)
    , testnet_(testnet)
    , rate_limit_window_(std::chrono::seconds(1))
    , rate_limit_per_window_(1200)
    , max_retries_(3)
    , retry_delay_(std::chrono::milliseconds(1000)) {
    if (testnet_) {
        base_rest_url_ = "https://testnet.binance.vision";
        base_ws_url_ = "wss://testnet.binance.vision";
    } else {
        base_rest_url_ = "https://api.binance.com";
        base_ws_url_ = "wss://stream.binance.com:9443";
    }

#ifndef VELOZ_NO_CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
}

BinanceAdapter::~BinanceAdapter() {
    disconnect();
#ifndef VELOZ_NO_CURL
    curl_global_cleanup();
#endif
}

std::string BinanceAdapter::build_signature(const std::string& query_string) {
#ifndef VELOZ_NO_OPENSSL
    // Implement HMAC-SHA256 signature using OpenSSL
    unsigned char* digest;
    unsigned int digest_len;

    digest = HMAC(EVP_sha256(),
                 secret_key_.c_str(), secret_key_.length(),
                 reinterpret_cast<const unsigned char*>(query_string.c_str()), query_string.length(),
                 nullptr, &digest_len);

    // Convert binary digest to hex string
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(2);
    for (unsigned int i = 0; i < digest_len; ++i) {
        oss << static_cast<unsigned int>(digest[i]);
    }

    return oss.str();
#else
    // Placeholder for builds without OpenSSL
    // In production, this should be implemented with a proper crypto library
    (void)query_string;
    std::cerr << "Warning: OpenSSL not available, using placeholder signature" << std::endl;
    return "placeholder_signature";
#endif
}

std::string BinanceAdapter::http_get(const std::string& endpoint, const std::string& params) {
#ifdef VELOZ_NO_CURL
    (void)endpoint;
    (void)params;
    return "";
#else
    CURL* curl = curl_easy_init();
    if (!curl) {
        return "";
    }

    std::string url = base_rest_url_ + endpoint;
    if (!params.empty()) {
        url += "?" + params;
    }

    std::string response_string;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    if (!api_key_.empty()) {
        std::string auth_header = "X-MBX-APIKEY: " + api_key_;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, auth_header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "HTTP GET failed: " << curl_easy_strerror(res) << std::endl;
    }

    curl_easy_cleanup(curl);
    last_activity_time_ = std::chrono::steady_clock::now();

    return response_string;
#endif
}

std::string BinanceAdapter::http_post(const std::string& endpoint, const std::string& params) {
#ifdef VELOZ_NO_CURL
    (void)endpoint;
    (void)params;
    return "";
#else
    CURL* curl = curl_easy_init();
    if (!curl) {
        return "";
    }

    std::string url = base_rest_url_ + endpoint;

    std::string response_string;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    if (!api_key_.empty()) {
        std::string auth_header = "X-MBX-APIKEY: " + api_key_;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, auth_header.c_str());
        headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "HTTP POST failed: " << curl_easy_strerror(res) << std::endl;
    }

    curl_easy_cleanup(curl);
    last_activity_time_ = std::chrono::steady_clock::now();

    return response_string;
#endif
}

std::string BinanceAdapter::http_delete(const std::string& endpoint, const std::string& params) {
#ifdef VELOZ_NO_CURL
    (void)endpoint;
    (void)params;
    return "";
#else
    CURL* curl = curl_easy_init();
    if (!curl) {
        return "";
    }

    std::string url = base_rest_url_ + endpoint;
    if (!params.empty()) {
        url += "?" + params;
    }

    std::string response_string;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_string);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    if (!api_key_.empty()) {
        std::string auth_header = "X-MBX-APIKEY: " + api_key_;
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, auth_header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::cerr << "HTTP DELETE failed: " << curl_easy_strerror(res) << std::endl;
    }

    curl_easy_cleanup(curl);
    last_activity_time_ = std::chrono::steady_clock::now();

    return response_string;
#endif
}

std::string BinanceAdapter::format_symbol(const veloz::common::SymbolId& symbol) {
    std::string formatted = symbol.value;
    std::transform(formatted.begin(), formatted.end(), formatted.begin(), ::toupper);
    return formatted;
}

std::string BinanceAdapter::order_side_to_string(OrderSide side) {
    return side == OrderSide::Buy ? "BUY" : "SELL";
}

std::string BinanceAdapter::order_type_to_string(OrderType type) {
    switch (type) {
        case OrderType::Market:
            return "MARKET";
        case OrderType::Limit:
            return "LIMIT";
        default:
            return "LIMIT";
    }
}

std::string BinanceAdapter::time_in_force_to_string(TimeInForce tif) {
    switch (tif) {
        case TimeInForce::GTC:
            return "GTC";
        case TimeInForce::IOC:
            return "IOC";
        case TimeInForce::FOK:
            return "FOK";
        case TimeInForce::GTX:
            return "GTX";
        default:
            return "GTC";
    }
}

OrderStatus BinanceAdapter::parse_order_status(const std::string& status_str) {
    if (status_str == "NEW") {
        return OrderStatus::New;
    } else if (status_str == "PARTIALLY_FILLED") {
        return OrderStatus::PartiallyFilled;
    } else if (status_str == "FILLED") {
        return OrderStatus::Filled;
    } else if (status_str == "CANCELED" || status_str == "PENDING_CANCEL") {
        return OrderStatus::Canceled;
    } else if (status_str == "REJECTED") {
        return OrderStatus::Rejected;
    } else if (status_str == "EXPIRED") {
        return OrderStatus::Expired;
    }
    return OrderStatus::New;
}

std::optional<ExecutionReport> BinanceAdapter::place_order(const PlaceOrderRequest& req) {
    if (!is_connected()) {
        connect();
        if (!is_connected()) {
            return std::nullopt;
        }
    }

    std::string endpoint = "/api/v3/order";

    std::ostringstream params;
    params << "symbol=" << format_symbol(req.symbol)
           << "&side=" << order_side_to_string(req.side)
           << "&type=" << order_type_to_string(req.type)
           << "&timeInForce=" << time_in_force_to_string(req.tif)
           << "&quantity=" << req.qty;

    if (req.price) {
        params << "&price=" << *req.price;
    }

    if (!req.client_order_id.empty()) {
        params << "&newClientOrderId=" << req.client_order_id;
    }

    params << "&timestamp=" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (req.reduce_only) {
        params << "&reduceOnly=true";
    }

    if (req.post_only) {
        params << "&postOnly=true";
    }

    std::string query_string = params.str();
    std::string signature = build_signature(query_string);
    query_string += "&signature=" + signature;

    std::string response = http_post(endpoint, query_string);

    try {
        auto doc = JsonDocument::parse(response);
        auto root = doc.root();

        ExecutionReport report;
        report.symbol = req.symbol;
        report.client_order_id = req.client_order_id;
        report.venue_order_id = root["orderId"].get_string("");
        report.status = parse_order_status(root["status"].get_string("NEW"));
        report.last_fill_qty = root["executedQty"].get_double(0.0);
        report.last_fill_price = root["price"].get_double(0.0);
        report.ts_exchange_ns = root["transactTime"].get_int(0LL) * 1000000;
        report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        return report;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing place order response: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<ExecutionReport> BinanceAdapter::cancel_order(const CancelOrderRequest& req) {
    if (!is_connected()) {
        connect();
        if (!is_connected()) {
            return std::nullopt;
        }
    }

    std::string endpoint = "/api/v3/order";

    std::ostringstream params;
    params << "symbol=" << format_symbol(req.symbol)
           << "&origClientOrderId=" << req.client_order_id
           << "&timestamp=" << std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count();

    std::string query_string = params.str();
    std::string signature = build_signature(query_string);
    query_string += "&signature=" + signature;

    std::string response = http_delete(endpoint, query_string);

    try {
        auto doc = JsonDocument::parse(response);
        auto root = doc.root();

        ExecutionReport report;
        report.symbol = req.symbol;
        report.client_order_id = req.client_order_id;
        report.venue_order_id = root["orderId"].get_string("");
        report.status = parse_order_status(root["status"].get_string("CANCELED"));
        report.last_fill_qty = root["executedQty"].get_double(0.0);
        report.last_fill_price = root["price"].get_double(0.0);
        report.ts_exchange_ns = root["transactTime"].get_int(0LL) * 1000000;
        report.ts_recv_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();

        return report;
    } catch (const std::exception& e) {
        std::cerr << "Error parsing cancel order response: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool BinanceAdapter::is_connected() const {
    if (!connected_) {
        return false;
    }

    // Check if last activity is within reasonable time
    auto now = std::chrono::steady_clock::now();
    auto idle_time = std::chrono::duration_cast<std::chrono::seconds>(
        now - last_activity_time_);

    return idle_time.count() < 30; // Assume disconnected if idle for 30 seconds
}

void BinanceAdapter::connect() {
    if (connected_) {
        return;
    }

    // Test connection by getting server time
    std::string response = http_get("/api/v3/time");

    try {
        auto doc = JsonDocument::parse(response);
        auto root = doc.root();
        if (root["serverTime"].is_valid()) {
            connected_ = true;
            last_activity_time_ = std::chrono::steady_clock::now();
            std::cout << "Binance API connected successfully" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error connecting to Binance API: " << e.what() << std::endl;
        connected_ = false;
    }
}

void BinanceAdapter::disconnect() {
    connected_ = false;
    std::cout << "Binance API disconnected" << std::endl;
}

const char* BinanceAdapter::name() const {
    return "Binance";
}

const char* BinanceAdapter::version() const {
    return "1.0.0";
}

std::optional<double> BinanceAdapter::get_current_price(const veloz::common::SymbolId& symbol) {
    std::string endpoint = "/api/v3/ticker/price";
    std::string params = "symbol=" + format_symbol(symbol);

    std::string response = http_get(endpoint, params);

    try {
        auto doc = JsonDocument::parse(response);
        auto root = doc.root();
        auto price_val = root["price"];
        if (price_val.is_string()) {
            return std::stod(price_val.get_string());
        } else if (price_val.is_real()) {
            return price_val.get_double();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting current price: " << e.what() << std::endl;
    }

    return std::nullopt;
}

std::optional<std::map<double, double>> BinanceAdapter::get_order_book(const veloz::common::SymbolId& symbol, int depth) {
    std::string endpoint = "/api/v3/depth";
    std::string params = "symbol=" + format_symbol(symbol) + "&limit=" + std::to_string(depth);

    std::string response = http_get(endpoint, params);

    try {
        auto doc = JsonDocument::parse(response);
        auto root = doc.root();

        std::map<double, double> order_book;

        auto bids = root["bids"];
        if (bids.is_array()) {
            for (size_t i = 0; i < bids.size(); ++i) {
                auto bid = bids[i];
                double price = std::stod(bid[0].get_string());
                double qty = std::stod(bid[1].get_string());
                order_book[price] = qty;
            }
        }

        auto asks = root["asks"];
        if (asks.is_array()) {
            for (size_t i = 0; i < asks.size(); ++i) {
                auto ask = asks[i];
                double price = std::stod(ask[0].get_string());
                double qty = std::stod(ask[1].get_string());
                order_book[price] = qty;
            }
        }

        return order_book;
    } catch (const std::exception& e) {
        std::cerr << "Error getting order book: " << e.what() << std::endl;
    }

    return std::nullopt;
}

std::optional<std::vector<std::pair<double, double>>> BinanceAdapter::get_recent_trades(const veloz::common::SymbolId& symbol, int limit) {
    std::string endpoint = "/api/v3/trades";
    std::string params = "symbol=" + format_symbol(symbol) + "&limit=" + std::to_string(limit);

    std::string response = http_get(endpoint, params);

    try {
        auto doc = JsonDocument::parse(response);
        auto root = doc.root();

        std::vector<std::pair<double, double>> trades;

        for (size_t i = 0; i < root.size(); ++i) {
            auto trade = root[i];
            double price = std::stod(trade["price"].get_string());
            double qty = std::stod(trade["qty"].get_string());
            trades.emplace_back(price, qty);
        }

        return trades;
    } catch (const std::exception& e) {
        std::cerr << "Error getting recent trades: " << e.what() << std::endl;
    }

    return std::nullopt;
}

std::optional<double> BinanceAdapter::get_account_balance(const std::string& asset) {
    std::string endpoint = "/api/v3/account";

    std::ostringstream params;
    params << "timestamp=" << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::string query_string = params.str();
    std::string signature = build_signature(query_string);
    query_string += "&signature=" + signature;

    std::string response = http_get(endpoint, query_string);

    try {
        auto doc = JsonDocument::parse(response);
        auto root = doc.root();

        auto balances = root["balances"];
        if (balances.is_array()) {
            for (size_t i = 0; i < balances.size(); ++i) {
                auto balance = balances[i];
                if (balance["asset"].get_string("") == asset) {
                    return std::stod(balance["free"].get_string());
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error getting account balance: " << e.what() << std::endl;
    }

    return std::nullopt;
}

} // namespace veloz::exec
