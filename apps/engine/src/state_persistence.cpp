#include "veloz/engine/state_persistence.h"

#include "veloz/core/json.h"
#include "veloz/core/time.h"
#include "veloz/strategy/strategy.h"

#include <algorithm>
#include <charconv>
#include <cstring>
#include <format>
#include <kj/debug.h>
#include <kj/string.h>

namespace veloz::engine {

namespace {

// Current snapshot format version
constexpr std::uint32_t kCurrentVersion = 1;

// Snapshot file extension
constexpr const char* kSnapshotExtension = ".snapshot.json";

} // namespace

StatePersistence::StatePersistence(StatePersistenceConfig config)
    : config_(kj::mv(config)), fs_(kj::newDiskFilesystem()) {}

StatePersistence::~StatePersistence() {
  stop_periodic_snapshots();
}

bool StatePersistence::initialize() {
  try {
    // Create snapshot directory if it doesn't exist
    // Using openSubdir with CREATE | CREATE_PARENT acts like mkdir -p
    fs_->getCurrent().openSubdir(config_.snapshot_dir,
                                 kj::WriteMode::CREATE | kj::WriteMode::CREATE_PARENT);

    // Load the latest sequence number from existing snapshots
    auto files = get_snapshot_files();
    if (files.size() > 0) {
      // Parse sequence numbers from filenames and find the max
      std::uint64_t max_seq = 0;
      for (const auto& path : files) {
        // basename() returns Path, need to get string from it
        // kj::Path isn't directly convertible to string for manipulation easily
        kj::StringPtr filename = path.basename()[0];

        // Format: snapshot_NNNNNNNNNN.snapshot.json
        if (filename.startsWith("snapshot_") && filename.endsWith(kSnapshotExtension)) {
          // "snapshot_" is 9 chars
          auto seq_str = filename.slice(9, filename.size() - std::strlen(kSnapshotExtension));
          std::uint64_t seq = 0;
          auto res = std::from_chars(seq_str.begin(), seq_str.end(), seq);
          if (res.ec == std::errc{}) {
            max_seq = std::max(max_seq, seq);
          }
        }
      }
      *sequence_num_.lockExclusive() = max_seq;
    }

    return true;
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Failed to initialize state persistence", e);
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

  return snapshot;
}

bool StatePersistence::save_snapshot(const StateSnapshot& snapshot) {
  try {
    // Serialize to JSON
    kj::String json = serialize_snapshot(snapshot);

    // Update sequence number
    *sequence_num_.lockExclusive() = snapshot.meta.sequence_num;

    // Get file path
    auto path = get_snapshot_path(snapshot.meta.sequence_num);

    // Write to file
    fs_->getCurrent().openFile(path, kj::WriteMode::CREATE | kj::WriteMode::MODIFY)->writeAll(json);

    // Cleanup old snapshots
    const_cast<StatePersistence*>(this)->cleanup_old_snapshots();

    return true;
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Failed to save snapshot", e);
    return false;
  }
}

kj::Maybe<StateSnapshot> StatePersistence::load_latest_snapshot() const {
  auto files = get_snapshot_files();
  if (files.size() == 0) {
    return kj::none;
  }

  // Files are sorted by modification time (or name), get the latest
  auto& latest_path = files[files.size() - 1];

  try {
    auto content = fs_->getCurrent().openFile(latest_path)->readAllText();
    return deserialize_snapshot(content);
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Failed to read snapshot file", latest_path.toString(), e);
    return kj::none;
  }
}

kj::Maybe<StateSnapshot> StatePersistence::load_snapshot(std::uint64_t sequence_num) const {
  auto path = get_snapshot_path(sequence_num);

  try {
    // Check existence by trying to open
    if (!fs_->getCurrent().exists(path)) {
      return kj::none;
    }

    auto content = fs_->getCurrent().openFile(path)->readAllText();
    return deserialize_snapshot(content);
  } catch (...) {
    return kj::none;
  }
}

bool StatePersistence::restore_state(const StateSnapshot& snapshot, EngineState& state) const {
  // Restore price
  state.set_price(snapshot.price);

  return true;
}

void StatePersistence::cleanup_old_snapshots() {
  auto files = get_snapshot_files();
  if (files.size() <= config_.max_snapshots) {
    return;
  }

  // Remove oldest snapshots
  size_t to_remove = files.size() - config_.max_snapshots;
  for (size_t i = 0; i < to_remove; ++i) {
    try {
      fs_->getCurrent().remove(files[i]);
    } catch (const kj::Exception& e) {
      KJ_LOG(WARNING, "Failed to remove old snapshot", files[i].toString(), e);
    }
  }
}

kj::Vector<StateSnapshotMeta> StatePersistence::list_snapshots() const {
  kj::Vector<StateSnapshotMeta> result;

  auto files = get_snapshot_files();
  for (const auto& path : files) {
    try {
      auto content = fs_->getCurrent().openFile(path)->readAllText();
      auto doc = veloz::core::JsonDocument::parse(content);
      auto root = doc.root();

      StateSnapshotMeta meta;
      meta.version = static_cast<std::uint32_t>(root["version"].get_uint(1));
      meta.timestamp_ns = root["timestamp_ns"].get_int(0);
      meta.sequence_num = root["sequence_num"].get_uint(0);
      result.add(kj::mv(meta));
    } catch (...) {
      // Ignore invalid files
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

kj::Path StatePersistence::get_snapshot_path(std::uint64_t sequence_num) const {
  // std::format for width specifiers
  auto filename = std::format("snapshot_{:010d}{}", sequence_num, kSnapshotExtension);
  return config_.snapshot_dir.append(filename.c_str());
}

kj::Vector<kj::Path> StatePersistence::get_snapshot_files() const {
  kj::Vector<kj::Path> files;

  try {
    if (!fs_->getCurrent().exists(config_.snapshot_dir)) {
      return files;
    }

    auto dir = fs_->getCurrent().openSubdir(config_.snapshot_dir);
    auto names = dir->listNames();

    for (auto& name : names) {
      if (name.endsWith(kSnapshotExtension)) {
        files.add(config_.snapshot_dir.append(name));
      }
    }

    // Sort by filename (which includes sequence number)
    std::sort(files.begin(), files.end());
  } catch (const kj::Exception& e) {
    KJ_LOG(WARNING, "Failed to list snapshot files", e);
  }

  return files;
}

kj::String StatePersistence::compute_checksum(kj::StringPtr data) const {
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
    auto filename = kj::str("strategy_", strategy_id, ".json");
    auto strategy_path = config_.snapshot_dir.append(filename);

    kj::String json =
        kj::str(R"({"strategy_id":")", state.strategy_id, R"(","strategy_name":")",
                state.strategy_name, R"(,"is_running":)", state.is_running ? "true" : "false",
                R"(,"pnl":)", state.pnl, R"(,"total_pnl":)", state.total_pnl, R"(,"max_drawdown":)",
                state.max_drawdown, R"(,"trade_count":)", state.trade_count, R"(,"win_count":)",
                state.win_count, R"(,"lose_count":)", state.lose_count, R"(,"win_rate":)",
                state.win_rate, R"(,"profit_factor":)", state.profit_factor, R"(})");

    fs_->getCurrent()
        .openFile(strategy_path, kj::WriteMode::CREATE | kj::WriteMode::MODIFY)
        ->writeAll(json);

    KJ_LOG(INFO, "Saved strategy state", strategy_id);
    return true;
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Failed to save strategy state", e);
    return false;
  }
}

kj::Maybe<strategy::StrategyState>
StatePersistence::load_strategy_state(kj::StringPtr strategy_id) const {
  try {
    auto filename = kj::str("strategy_", strategy_id, ".json");
    auto strategy_path = config_.snapshot_dir.append(filename);

    if (!fs_->getCurrent().exists(strategy_path)) {
      return kj::none;
    }

    auto content = fs_->getCurrent().openFile(strategy_path)->readAllText();
    auto doc = veloz::core::JsonDocument::parse(content);
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

    return state;
  } catch (...) {
    return kj::none;
  }
}

bool StatePersistence::remove_strategy_state(kj::StringPtr strategy_id) {
  try {
    auto filename = kj::str("strategy_", strategy_id, ".json");
    auto strategy_path = config_.snapshot_dir.append(filename);

    if (fs_->getCurrent().exists(strategy_path)) {
      fs_->getCurrent().remove(strategy_path);
      KJ_LOG(INFO, "Removed strategy state", strategy_id);
      return true;
    }
    return false;
  } catch (const kj::Exception& e) {
    KJ_LOG(ERROR, "Failed to remove strategy state", e);
    return false;
  }
}

kj::Vector<kj::String> StatePersistence::list_strategy_states() const {
  kj::Vector<kj::String> result;

  try {
    if (!fs_->getCurrent().exists(config_.snapshot_dir)) {
      return result;
    }

    auto dir = fs_->getCurrent().openSubdir(config_.snapshot_dir);
    auto names = dir->listNames();

    const kj::StringPtr prefix = "strategy_";
    const kj::StringPtr suffix = ".json";

    for (auto& name : names) {
      if (name.startsWith(prefix) && name.endsWith(suffix)) {
        // Extract strategy_id from filename: strategy_<id>.json
        auto strategy_id = name.slice(prefix.size(), name.size() - suffix.size());
        result.add(kj::heapString(strategy_id));
      }
    }
  } catch (const kj::Exception& e) {
    KJ_LOG(WARNING, "Failed to list strategy states", e);
  }

  return result;
}

} // namespace veloz::engine
