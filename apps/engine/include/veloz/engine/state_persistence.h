#pragma once

#include "veloz/engine/engine_state.h"
#include "veloz/strategy/strategy.h"

#include <kj/async-io.h>
#include <kj/filesystem.h>
#include <kj/mutex.h>
#include <kj/string.h>

// std::filesystem for path operations (KJ lacks filesystem API)
#include <filesystem>

namespace veloz::engine {

/**
 * @brief State snapshot metadata
 */
struct StateSnapshotMeta {
  std::uint32_t version{1};      ///< Snapshot format version
  std::int64_t timestamp_ns{0};  ///< Snapshot creation timestamp
  std::uint64_t sequence_num{0}; ///< Monotonic sequence number
  kj::String checksum;           ///< SHA-256 checksum of data

  StateSnapshotMeta() : checksum(kj::str("")) {}
};

/**
 * @brief Serialized strategy state for persistence
 */
struct StrategySnapshot {
  kj::String strategy_id;          ///< Strategy ID
  kj::String strategy_name;        ///< Strategy name
  strategy::StrategyType type;     ///< Strategy type
  strategy::StrategyStatus status; ///< Strategy status
  kj::String config_json;          ///< Strategy configuration (serialized)
  kj::String state_json;           ///< Strategy runtime state (serialized)
};

/**
 * @brief Serialized state snapshot
 */
struct StateSnapshot {
  StateSnapshotMeta meta;

  // Balances
  kj::Vector<Balance> balances;

  // Pending orders
  kj::Vector<PendingOrder> pending_orders;

  // Current price
  double price{0.0};

  // Venue counter
  std::uint64_t venue_counter{0};

  // Strategy states
  kj::Vector<StrategySnapshot> strategies;
};

/**
 * @brief Configuration for state persistence
 */
struct StatePersistenceConfig {
  std::filesystem::path snapshot_dir{"./snapshots"}; ///< Directory for snapshots
  std::int64_t snapshot_interval_ms{60000};          ///< Snapshot interval (default: 1 minute)
  std::uint32_t max_snapshots{10};                   ///< Maximum snapshots to keep
  bool enable_compression{false};                    ///< Enable snapshot compression
};

/**
 * @brief Engine state persistence manager
 *
 * Handles serialization, periodic snapshots, and restoration of engine state.
 */
class StatePersistence final {
public:
  /**
   * @brief Construct state persistence manager
   * @param config Persistence configuration
   */
  explicit StatePersistence(StatePersistenceConfig config);

  ~StatePersistence();

  // Disable copy and move
  KJ_DISALLOW_COPY_AND_MOVE(StatePersistence);

  /**
   * @brief Initialize persistence (create directories, etc.)
   * @return true if initialization succeeded
   */
  bool initialize();

  /**
   * @brief Create snapshot from engine state
   * @param state Engine state to snapshot
   * @return Snapshot object
   */
  StateSnapshot create_snapshot(const EngineState& state) const;

  /**
   * @brief Save snapshot to disk
   * @param snapshot Snapshot to save
   * @return true if save succeeded
   */
  bool save_snapshot(const StateSnapshot& snapshot);

  /**
   * @brief Load latest snapshot from disk
   * @return Snapshot if found, empty if not
   */
  kj::Maybe<StateSnapshot> load_latest_snapshot() const;

  /**
   * @brief Load snapshot by sequence number
   * @param sequence_num Sequence number
   * @return Snapshot if found, empty if not
   */
  kj::Maybe<StateSnapshot> load_snapshot(std::uint64_t sequence_num) const;

  /**
   * @brief Restore engine state from snapshot
   * @param snapshot Snapshot to restore from
   * @param state Engine state to restore into
   * @return true if restoration succeeded
   */
  bool restore_state(const StateSnapshot& snapshot, EngineState& state) const;

  /**
   * @brief Clean up old snapshots (keep only max_snapshots)
   */
  void cleanup_old_snapshots();

  /**
   * @brief Get list of available snapshots
   * @return Vector of snapshot metadata
   */
  kj::Vector<StateSnapshotMeta> list_snapshots() const;

  /**
   * @brief Get current sequence number
   */
  std::uint64_t get_sequence_number() const;

  /**
   * @brief Start periodic snapshot timer
   * @param timer KJ timer for scheduling
   * @param state Engine state to snapshot
   * @return Promise that completes when stopped
   */
  kj::Promise<void> start_periodic_snapshots(kj::Timer& timer, const EngineState& state);

  /**
   * @brief Stop periodic snapshots
   */
  void stop_periodic_snapshots();

  // Strategy persistence methods

  /**
   * @brief Save strategy state snapshot
   * @param strategy_id Strategy ID
   * @param state Strategy state to save
   * @return true if save succeeded
   */
  bool save_strategy_state(kj::StringPtr strategy_id, const strategy::StrategyState& state);

  /**
   * @brief Load strategy state snapshot
   * @param strategy_id Strategy ID
   * @return Strategy state if found, empty otherwise
   */
  kj::Maybe<strategy::StrategyState> load_strategy_state(kj::StringPtr strategy_id) const;

  /**
   * @brief Remove strategy state snapshot
   * @param strategy_id Strategy ID
   * @return true if removal succeeded
   */
  bool remove_strategy_state(kj::StringPtr strategy_id);

  /**
   * @brief List all strategy state IDs
   * @return Vector of strategy IDs
   */
  kj::Vector<kj::String> list_strategy_states() const;

private:
  StatePersistenceConfig config_;
  kj::MutexGuarded<std::uint64_t> sequence_num_{0};
  kj::MutexGuarded<bool> running_{false};

  // Serialization helpers
  kj::String serialize_snapshot(const StateSnapshot& snapshot) const;
  kj::Maybe<StateSnapshot> deserialize_snapshot(kj::StringPtr json) const;

  // File helpers
  std::filesystem::path get_snapshot_path(std::uint64_t sequence_num) const;
  kj::Vector<std::filesystem::path> get_snapshot_files() const;

  // Checksum helpers
  kj::String compute_checksum(kj::StringPtr data) const;
  bool verify_checksum(kj::StringPtr data, kj::StringPtr expected) const;
};

} // namespace veloz::engine
