#include "veloz/engine/state_persistence.h"

#include "veloz/core/json.h"
#include "veloz/core/time.h"
#include "veloz/strategy/strategy.h"

#include <kj/debug.h>
#include <kj/string.h>

// std::filesystem for directory operations (KJ lacks filesystem API)
#include <algorithm>
#include <fstream>

namespace veloz::engine {

namespace {

// Current snapshot format version
constexpr std::uint32_t kCurrentVersion = 1;

// Snapshot file extension
constexpr const char* kSnapshotExtension = ".snapshot.json";

// Helper function to load snapshot metadata without full deserialization
kj::Maybe<StateSnapshotMeta> load_snapshot_meta_from_path(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return kj::none;
  }

  auto size = file.tellg();
  file.seekg(0, std::ios::beg);

  // std::string for file I/O
  std::string content(size, '\0');
  file.read(content.data(), size);
  file.close();

  try {
    auto doc = veloz::core::JsonDocument::parse(content.c_str());
    auto root = doc.root();
    StateSnapshotMeta meta;
    meta.version = static_cast<std::uint32_t>(root["version"].get_uint(1));
    meta.timestamp_ns = root["timestamp_ns"].get_int(0);
    meta.sequence_num = root["sequence_num"].get_uint(0);
    return meta;
  } catch (...) {
    return kj::none;
  }
}

} // namespace

StatePersistence::StatePersistence(StatePersistenceConfig config) : config_(kj::mv(config)) {}

StatePersistence::~StatePersistence() {
  stop_periodic_snapshots();
}

bool StatePersistence::initialize() {
  try {
    // Create snapshot directory if it doesn't exist
    if (!std::filesystem::exists(config_.snapshot_dir)) {
      std::filesystem::create_directories(config_.snapshot_dir);
    }

    // Load the latest sequence number from existing snapshots
    auto files = get_snapshot_files();
    if (files.size() > 0) {
      // Parse sequence numbers from filenames and find the max
      std::uint64_t max_seq = 0;
      for (const auto& path : files) {
        auto filename = path.filename().string();
        // Format: snapshot_NNNNNNNNNN.snapshot.json
        if (filename.starts_with("snapshot_") && filename.ends_with(kSnapshotExtension)) {
          auto seq_str = filename.substr(9, filename.length() - 9 - strlen(kSnapshotExtension));
          try {
            auto seq = std::stoull(seq_str);
            max_seq = std::max(max_seq, seq);
          } catch (...) {
            // Ignore invalid filenames
          }
        }
      }
      *sequence_num_.lockExclusive() = max_seq;
    }

    return true;
  } catch (const std::exception& e) {
    KJ_LOG(ERROR, "Failed to initialize state persistence", kj::str(e.what()).cStr());
    return false;
  }
}

StateSnapshot StatePersistence::create_snapshot(const EngineState& state) const {
  StateSnapshot snapshot;

  // Set metadata
  snapshot.meta.version = kCurrentVersion;
  snapshot.meta.timestamp_ns = veloz::core::now_unix_ns();
  snapshot.meta.sequence_num = *sequence_num_.lockShared() + 1;

  // Copy balances
  auto balances = state.snapshot_balances();
  for (auto& balance : balances) {
    Balance b;
    b.asset = kj::str(balance.asset);
    b.free = balance.free;
    b.locked = balance.locked;
    snapshot.balances.add(kj::mv(b));
  }

  // Copy price
  snapshot.price = state.price();

  // Note: pending_orders and venue_counter are private in EngineState
  // They would need accessor methods to be included in snapshot

  return snapshot;
}

bool StatePersistence::save_snapshot(const StateSnapshot& snapshot) {
  try {
    // Serialize to JSON
    kj::String json = serialize_snapshot(snapshot);

    // Compute checksum
    kj::String checksum = compute_checksum(json);

    // Update sequence number
    *sequence_num_.lockExclusive() = snapshot.meta.sequence_num;

    // Get file path
    auto path = get_snapshot_path(snapshot.meta.sequence_num);

    // Write to file
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
      KJ_LOG(ERROR, "Failed to open snapshot file for writing",
             kj::str(path.string().c_str()).cStr());
      return false;
    }

    file.write(json.cStr(), json.size());
    file.close();

    // Cleanup old snapshots
    const_cast<StatePersistence*>(this)->cleanup_old_snapshots();

    return true;
  } catch (const std::exception& e) {
    KJ_LOG(ERROR, "Failed to save snapshot", kj::str(e.what()).cStr());
    return false;
  }
}

kj::Maybe<StateSnapshot> StatePersistence::load_latest_snapshot() const {
  auto files = get_snapshot_files();
  if (files.size() == 0) {
    return kj::none;
  }

  // Files are sorted by modification time, get the latest
  auto& latest_path = files[files.size() - 1];

  // Read file content
  std::ifstream file(latest_path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    KJ_LOG(ERROR, "Failed to open snapshot file for reading",
           kj::str(latest_path.string().c_str()).cStr());
    return kj::none;
  }

  auto size = file.tellg();
  file.seekg(0, std::ios::beg);

  // std::string for file I/O (std::ifstream requires it)
  std::string content(size, '\0');
  file.read(content.data(), size);
  file.close();

  return deserialize_snapshot(content.c_str());
}

kj::Maybe<StateSnapshot> StatePersistence::load_snapshot(std::uint64_t sequence_num) const {
  auto path = get_snapshot_path(sequence_num);
  if (!std::filesystem::exists(path)) {
    return kj::none;
  }

  // Read file content
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return kj::none;
  }

  auto size = file.tellg();
  file.seekg(0, std::ios::beg);

  // std::string for file I/O (std::ifstream requires it)
  std::string content(size, '\0');
  file.read(content.data(), size);
  file.close();

  return deserialize_snapshot(content.c_str());
}

bool StatePersistence::restore_state(const StateSnapshot& snapshot, EngineState& state) const {
  // Restore price
  state.set_price(snapshot.price);

  // Note: Restoring balances and pending orders would require additional
  // accessor methods in EngineState that don't currently exist

  return true;
}

void StatePersistence::cleanup_old_snapshots() {
  auto files = get_snapshot_files();
  if (files.size() <= config_.max_snapshots) {
    return;
  }

  // Remove oldest snapshots (files are sorted by modification time)
  size_t to_remove = files.size() - config_.max_snapshots;
  for (size_t i = 0; i < to_remove; ++i) {
    try {
      std::filesystem::remove(files[i]);
    } catch (const std::exception& e) {
      KJ_LOG(WARNING, "Failed to remove old snapshot", kj::str(files[i].string().c_str()).cStr(),
             kj::str(e.what()).cStr());
    }
  }
}

kj::Vector<StateSnapshotMeta> StatePersistence::list_snapshots() const {
  kj::Vector<StateSnapshotMeta> result;

  auto files = get_snapshot_files();
  for (const auto& path : files) {
    auto maybeMeta = load_snapshot_meta_from_path(path);
    KJ_IF_SOME(meta, maybeMeta) {
      result.add(kj::mv(meta));
    }
  }

  return result;
}

std::uint64_t StatePersistence::get_sequence_number() const {
  return *sequence_num_.lockShared();
}

kj::Promise<void> StatePersistence::start_periodic_snapshots(kj::Timer& timer,
                                                             const EngineState& state) {
  *running_.lockExclusive() = true;

  while (*running_.lockShared()) {
    // Wait for the snapshot interval
    co_await timer.afterDelay(config_.snapshot_interval_ms * kj::MILLISECONDS);

    if (!*running_.lockShared()) {
      break;
    }

    // Create and save snapshot
    auto snapshot = create_snapshot(state);
    if (!save_snapshot(snapshot)) {
      KJ_LOG(WARNING, "Failed to save periodic snapshot");
    }
  }
}

void StatePersistence::stop_periodic_snapshots() {
  *running_.lockExclusive() = false;
}

kj::String StatePersistence::serialize_snapshot(const StateSnapshot& snapshot) const {
  // Build JSON manually using kj::str
  kj::Vector<kj::String> balance_items;
  for (const auto& balance : snapshot.balances) {
    balance_items.add(kj::str(R"({"asset":")", balance.asset, R"(","free":)", balance.free,
                              R"(,"locked":)", balance.locked, R"(})"));
  }

  kj::String balances_json;
  if (balance_items.size() == 0) {
    balances_json = kj::str("[]");
  } else {
    // Manual join with comma
    kj::String joined = kj::str("");
    for (size_t i = 0; i < balance_items.size(); ++i) {
      if (i > 0) {
        joined = kj::str(joined, ",", balance_items[i]);
      } else {
        joined = kj::str(balance_items[i]);
      }
    }
    balances_json = kj::str("[", joined, "]");
  }

  return kj::str(R"({"version":)", snapshot.meta.version, R"(,"timestamp_ns":)",
                 snapshot.meta.timestamp_ns, R"(,"sequence_num":)", snapshot.meta.sequence_num,
                 R"(,"price":)", snapshot.price, R"(,"venue_counter":)", snapshot.venue_counter,
                 R"(,"balances":)", balances_json, R"(})");
}

kj::Maybe<StateSnapshot> StatePersistence::deserialize_snapshot(kj::StringPtr json) const {
  try {
    auto doc = veloz::core::JsonDocument::parse(json);
    auto root = doc.root();

    StateSnapshot snapshot;

    // Parse metadata
    snapshot.meta.version = static_cast<std::uint32_t>(root["version"].get_uint(1));
    snapshot.meta.timestamp_ns = root["timestamp_ns"].get_int(0);
    snapshot.meta.sequence_num = root["sequence_num"].get_uint(0);

    // Parse price
    snapshot.price = root["price"].get_double(0.0);

    // Parse venue counter
    snapshot.venue_counter = root["venue_counter"].get_uint(0);

    // Parse balances array
    auto balances_val = root["balances"];
    if (balances_val.is_array()) {
      for (size_t i = 0; i < balances_val.size(); ++i) {
        auto item = balances_val[i];
        Balance balance;
        balance.asset = item["asset"].get_string();
        balance.free = item["free"].get_double(0.0);
        balance.locked = item["locked"].get_double(0.0);
        snapshot.balances.add(kj::mv(balance));
      }
    }

    return snapshot;
  } catch (...) {
    return kj::none;
  }
}

std::filesystem::path StatePersistence::get_snapshot_path(std::uint64_t sequence_num) const {
  // std::format for width specifiers (kj::str lacks support)
  auto filename = std::format("snapshot_{:010d}{}", sequence_num, kSnapshotExtension);
  return config_.snapshot_dir / filename;
}

kj::Vector<std::filesystem::path> StatePersistence::get_snapshot_files() const {
  kj::Vector<std::filesystem::path> files;

  if (!std::filesystem::exists(config_.snapshot_dir)) {
    return files;
  }

  for (const auto& entry : std::filesystem::directory_iterator(config_.snapshot_dir)) {
    if (entry.is_regular_file()) {
      auto filename = entry.path().filename().string();
      if (filename.ends_with(kSnapshotExtension)) {
        files.add(entry.path());
      }
    }
  }

  // Sort by filename (which includes sequence number)
  // Using std::sort because kj::Vector doesn't have a sort method
  std::sort(files.begin(), files.end());

  return files;
}

kj::String StatePersistence::compute_checksum(kj::StringPtr data) const {
  // Simple checksum for now - could use SHA-256 in production
  std::uint64_t hash = 0;
  for (char c : data) {
    hash = hash * 31 + static_cast<std::uint64_t>(c);
  }
  return kj::str(hash);
}

bool StatePersistence::verify_checksum(kj::StringPtr data, kj::StringPtr expected) const {
  auto computed = compute_checksum(data);
  return computed == expected;
}

// Strategy persistence methods

bool StatePersistence::save_strategy_state(kj::StringPtr strategy_id,
                                           const strategy::StrategyState& state) {
  try {
    // Build filename using KJ string concatenation
    auto filename = kj::str("strategy_", strategy_id, ".json");
    auto strategy_file = config_.snapshot_dir / std::filesystem::path(filename.cStr());

    // Serialize strategy state to JSON
    kj::String json =
        kj::str(R"({"strategy_id":")", state.strategy_id, R"(","strategy_name":")",
                state.strategy_name, R"(","is_running":)", state.is_running ? "true" : "false",
                R"(,"pnl":)", state.pnl, R"(,"total_pnl":)", state.total_pnl, R"(,"max_drawdown":)",
                state.max_drawdown, R"(,"trade_count":)", state.trade_count, R"(,"win_count":)",
                state.win_count, R"(,"lose_count":)", state.lose_count, R"(,"win_rate":)",
                state.win_rate, R"(,"profit_factor":)", state.profit_factor, R"(})");

    std::ofstream file(strategy_file, std::ios::binary);
    if (!file.is_open()) {
      KJ_LOG(ERROR, "Failed to open strategy state file for writing",
             kj::str(strategy_file.string().c_str()).cStr());
      return false;
    }

    file.write(json.cStr(), json.size());
    file.close();

    KJ_LOG(INFO, "Saved strategy state", strategy_id.cStr());
    return true;
  } catch (const std::exception& e) {
    KJ_LOG(ERROR, "Failed to save strategy state", kj::str(e.what()).cStr());
    return false;
  }
}

kj::Maybe<strategy::StrategyState>
StatePersistence::load_strategy_state(kj::StringPtr strategy_id) const {
  try {
    // Build filename using KJ string concatenation
    auto filename = kj::str("strategy_", strategy_id, ".json");
    auto strategy_file = config_.snapshot_dir / std::filesystem::path(filename.cStr());

    if (!std::filesystem::exists(strategy_file)) {
      return kj::none;
    }

    std::ifstream file(strategy_file, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
      return kj::none;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string content(size, '\0');
    file.read(content.data(), size);
    file.close();

    auto doc = veloz::core::JsonDocument::parse(content.c_str());
    auto root = doc.root();

    strategy::StrategyState state;
    state.strategy_id = root["strategy_id"].get_string();
    state.strategy_name = root["strategy_name"].get_string();
    state.is_running = root["is_running"].get_bool(false);
    state.pnl = root["pnl"].get_double(0.0);
    state.total_pnl = root["total_pnl"].get_double(0.0);
    state.max_drawdown = root["max_drawdown"].get_double(0.0);
    state.trade_count = static_cast<int>(root["trade_count"].get_uint(0));
    state.win_count = static_cast<int>(root["win_count"].get_uint(0));
    state.lose_count = static_cast<int>(root["lose_count"].get_uint(0));
    state.win_rate = root["win_rate"].get_double(0.0);
    state.profit_factor = root["profit_factor"].get_double(0.0);

    // Return strategy state (implicitly converts to kj::Maybe<StrategyState>)
    strategy::StrategyState result = kj::mv(state);
    return result;
  } catch (...) {
    return kj::none;
  }
}

bool StatePersistence::remove_strategy_state(kj::StringPtr strategy_id) {
  try {
    // Build filename using KJ string concatenation
    auto filename = kj::str("strategy_", strategy_id, ".json");
    auto strategy_file = config_.snapshot_dir / std::filesystem::path(filename.cStr());

    if (!std::filesystem::exists(strategy_file)) {
      return false;
    }

    std::filesystem::remove(strategy_file);
    KJ_LOG(INFO, "Removed strategy state", strategy_id.cStr());
    return true;
  } catch (const std::exception& e) {
    KJ_LOG(ERROR, "Failed to remove strategy state", kj::str(e.what()).cStr());
    return false;
  }
}

kj::Vector<kj::String> StatePersistence::list_strategy_states() const {
  kj::Vector<kj::String> result;

  if (!std::filesystem::exists(config_.snapshot_dir)) {
    return result;
  }

  const std::string prefix = "strategy_";
  const std::string suffix = ".json";

  for (const auto& entry : std::filesystem::directory_iterator(config_.snapshot_dir)) {
    if (entry.is_regular_file()) {
      auto filename = entry.path().filename().string();
      if (filename.starts_with(prefix) && filename.ends_with(suffix)) {
        // Extract strategy_id from filename: strategy_<id>.json
        auto strategy_id =
            filename.substr(prefix.length(), filename.length() - prefix.length() - suffix.length());
        result.add(kj::heapString(strategy_id.c_str()));
      }
    }
  }

  return result;
}

} // namespace veloz::engine
