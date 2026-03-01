#pragma once

#include "veloz/common/types.h"
#include "veloz/exec/order_api.h"
#include "veloz/oms/order_record.h"

#include <cstdint>
#include <kj/async-io.h>
#include <kj/async.h>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/map.h>
#include <kj/memory.h>
#include <kj/mutex.h>
#include <kj/string.h>
#include <kj/time.h>
#include <kj/vector.h>

namespace veloz::exec {

// Reconciliation event types for audit trail
enum class ReconciliationEventType : std::uint8_t {
  Started = 0,
  Completed = 1,
  StateMismatch = 2,
  OrphanedOrderFound = 3,
  OrderCorrected = 4,
  OrderCancelled = 5,
  Error = 6,
  StrategyFrozen = 7,
  StrategyResumed = 8,
};

// Reconciliation action taken
enum class ReconciliationAction : std::uint8_t {
  None = 0,
  UpdateLocalState = 1,
  CancelOrphanedOrder = 2,
  FreezeStrategy = 3,
  ManualIntervention = 4,
};

// Severity level for reconciliation issues
enum class ReconciliationSeverity : std::uint8_t {
  Info = 0,     // Informational, no action needed
  Warning = 1,  // Minor discrepancy, auto-corrected
  Error = 2,    // Significant discrepancy, needs review
  Critical = 3, // Critical issue, requires manual intervention
};

// Mismatch details between local and exchange state
struct StateMismatch {
  kj::String client_order_id;
  kj::String symbol;
  OrderStatus local_status{OrderStatus::New};
  OrderStatus exchange_status{OrderStatus::New};
  double local_filled_qty{0.0};
  double exchange_filled_qty{0.0};
  double local_avg_price{0.0};
  double exchange_avg_price{0.0};
  ReconciliationAction action_taken{ReconciliationAction::None};
  ReconciliationSeverity severity{ReconciliationSeverity::Warning};
  std::int64_t detected_ts_ns{0};
  bool requires_manual_intervention{false};
  kj::String intervention_reason;
};

// Item requiring manual intervention
struct ManualInterventionItem {
  kj::String id;
  kj::String client_order_id;
  kj::String symbol;
  veloz::common::Venue venue{veloz::common::Venue::Unknown};
  kj::String description;
  ReconciliationSeverity severity{ReconciliationSeverity::Critical};
  std::int64_t created_ts_ns{0};
  std::int64_t resolved_ts_ns{0};
  bool resolved{false};
  kj::String resolution_notes;
};

// Position discrepancy between local and exchange
struct PositionDiscrepancy {
  kj::String symbol;
  veloz::common::Venue venue{veloz::common::Venue::Unknown};
  double local_qty{0.0};
  double exchange_qty{0.0};
  double qty_diff{0.0};
  double local_avg_price{0.0};
  double exchange_avg_price{0.0};
  ReconciliationSeverity severity{ReconciliationSeverity::Warning};
  std::int64_t detected_ts_ns{0};
};

// Reconciliation event for audit trail
struct ReconciliationEvent {
  ReconciliationEventType type{ReconciliationEventType::Started};
  std::int64_t ts_ns{0};
  kj::String message;
  kj::Maybe<StateMismatch> mismatch;
  kj::Maybe<kj::String> client_order_id;
  kj::Maybe<kj::String> error_message;
  ReconciliationSeverity severity{ReconciliationSeverity::Info};
};

// Reconciliation report - summary of a reconciliation cycle
struct ReconciliationReport {
  std::int64_t start_ts_ns{0};
  std::int64_t end_ts_ns{0};
  kj::Duration duration{0 * kj::NANOSECONDS};
  veloz::common::Venue venue{veloz::common::Venue::Unknown};

  // Order reconciliation results
  std::int64_t orders_checked{0};
  std::int64_t orders_matched{0};
  std::int64_t mismatches_found{0};
  std::int64_t mismatches_auto_resolved{0};
  std::int64_t orphaned_orders_found{0};
  std::int64_t orphaned_orders_cancelled{0};

  // Position reconciliation results
  std::int64_t positions_checked{0};
  std::int64_t position_discrepancies{0};

  // Manual intervention items
  std::int64_t manual_interventions_required{0};
  kj::Vector<ManualInterventionItem> intervention_items;

  // Detailed mismatches
  kj::Vector<StateMismatch> mismatches;
  kj::Vector<PositionDiscrepancy> position_discrepancies_list;

  // Overall status
  bool success{true};
  kj::String error_message;
  ReconciliationSeverity max_severity{ReconciliationSeverity::Info};
};

// Configuration for reconciliation loop
struct ReconciliationConfig {
  kj::Duration reconciliation_interval{30 * kj::SECONDS};
  kj::Duration stale_order_threshold{5 * kj::MINUTES};
  bool auto_cancel_orphaned{false};
  bool freeze_on_mismatch{true};
  int max_mismatches_before_freeze{3};
  int max_retries{3};
  kj::Duration retry_delay{1 * kj::SECONDS};
};

// Exchange order query interface - adapters implement this for reconciliation
class ReconciliationQueryInterface {
public:
  virtual ~ReconciliationQueryInterface() noexcept = default;

  // Query open orders from exchange
  virtual kj::Promise<kj::Vector<ExecutionReport>>
  query_open_orders_async(const veloz::common::SymbolId& symbol) = 0;

  // Query specific order by client order ID
  virtual kj::Promise<kj::Maybe<ExecutionReport>>
  query_order_async(const veloz::common::SymbolId& symbol, kj::StringPtr client_order_id) = 0;

  // Query all orders within a time window
  virtual kj::Promise<kj::Vector<ExecutionReport>>
  query_orders_async(const veloz::common::SymbolId& symbol, std::int64_t start_time_ms,
                     std::int64_t end_time_ms) = 0;

  // Cancel an order on the exchange (used for orphaned order cleanup)
  virtual kj::Promise<kj::Maybe<ExecutionReport>>
  cancel_order_async(const veloz::common::SymbolId& symbol, kj::StringPtr client_order_id) = 0;
};

// Callback for reconciliation events
using ReconciliationCallback = kj::Function<void(const ReconciliationEvent&)>;

// Strategy freeze callback - called when reconciliation detects critical issues
using StrategyFreezeCallback = kj::Function<void(bool freeze, kj::StringPtr reason)>;

// Account reconciliation loop
class AccountReconciler final {
public:
  AccountReconciler(kj::AsyncIoContext& io_context, oms::OrderStore& order_store,
                    ReconciliationConfig config = ReconciliationConfig{});

  // Register exchange adapter for reconciliation queries
  void register_exchange(veloz::common::Venue venue, ReconciliationQueryInterface& query_interface);

  // Unregister exchange adapter
  void unregister_exchange(veloz::common::Venue venue);

  // Start the reconciliation loop
  kj::Promise<void> start();

  // Stop the reconciliation loop
  void stop();

  // Force immediate reconciliation
  kj::Promise<void> reconcile_now();

  // Reconcile specific symbol
  kj::Promise<void> reconcile_symbol(veloz::common::Venue venue,
                                     const veloz::common::SymbolId& symbol);

  // Set callback for reconciliation events (audit trail)
  void set_event_callback(ReconciliationCallback callback);

  // Set callback for strategy freeze/resume
  void set_freeze_callback(StrategyFreezeCallback callback);

  // Get reconciliation statistics
  struct Stats {
    std::int64_t total_reconciliations{0};
    std::int64_t successful_reconciliations{0};
    std::int64_t failed_reconciliations{0};
    std::int64_t mismatches_detected{0};
    std::int64_t mismatches_corrected{0};
    std::int64_t orphaned_orders_found{0};
    std::int64_t orphaned_orders_cancelled{0};
    std::int64_t strategy_freezes{0};
    std::int64_t last_reconciliation_ts_ns{0};
    kj::Duration last_reconciliation_duration{0 * kj::NANOSECONDS};
  };

  [[nodiscard]] Stats get_stats() const;

  // Check if strategy is currently frozen
  [[nodiscard]] bool is_strategy_frozen() const;

  // Manually resume strategy (after manual intervention)
  void resume_strategy();

  // Get recent reconciliation events
  [[nodiscard]] kj::Vector<ReconciliationEvent> get_recent_events(size_t max_count = 100) const;

  // Generate reconciliation report for a specific venue
  [[nodiscard]] kj::Maybe<ReconciliationReport> get_last_report(veloz::common::Venue venue) const;

  // Get all pending manual intervention items
  [[nodiscard]] kj::Vector<ManualInterventionItem> get_pending_interventions() const;

  // Resolve a manual intervention item
  void resolve_intervention(kj::StringPtr intervention_id, kj::StringPtr resolution_notes);

  // Add a manual intervention item
  void add_manual_intervention(ManualInterventionItem item);

  // Get intervention item by ID
  [[nodiscard]] kj::Maybe<ManualInterventionItem> get_intervention(kj::StringPtr id) const;

  // Generate a text summary of the last reconciliation
  [[nodiscard]] kj::String generate_report_summary() const;

  // Export report as JSON string
  [[nodiscard]] kj::String export_report_json(const ReconciliationReport& report) const;

private:
  // Internal reconciliation loop
  kj::Promise<void> reconciliation_loop();

  // Reconcile orders for a specific venue
  kj::Promise<void> reconcile_venue(veloz::common::Venue venue);

  // Compare local and exchange order states
  void compare_order_states(const oms::OrderState& local, const ExecutionReport& exchange,
                            kj::Vector<StateMismatch>& mismatches);

  // Handle detected mismatches
  kj::Promise<void> handle_mismatches(veloz::common::Venue venue,
                                      kj::Vector<StateMismatch>& mismatches);

  // Handle orphaned orders (on exchange but not in local state)
  kj::Promise<void> handle_orphaned_orders(veloz::common::Venue venue,
                                           kj::Vector<ExecutionReport>& orphaned);

  // Update local state from exchange
  void update_local_state(const ExecutionReport& exchange_state);

  // Emit reconciliation event
  void emit_event(ReconciliationEvent event);

  // Freeze strategy due to critical issues
  void freeze_strategy(kj::StringPtr reason);

  // Get current timestamp in nanoseconds
  std::int64_t now_ns() const;

  // KJ async I/O context
  kj::AsyncIoContext& io_context_;

  // Order store reference
  oms::OrderStore& order_store_;

  // Configuration
  ReconciliationConfig config_;

  // Determine severity based on mismatch details
  ReconciliationSeverity determine_severity(const StateMismatch& mismatch) const;

  // Check if mismatch requires manual intervention
  bool requires_manual_intervention(const StateMismatch& mismatch) const;

  // Generate unique intervention ID
  kj::String generate_intervention_id() const;

  // Internal state
  struct ReconcilerState {
    kj::HashMap<veloz::common::Venue, ReconciliationQueryInterface*> exchanges;
    kj::Vector<ReconciliationEvent> event_history;
    kj::HashMap<veloz::common::Venue, ReconciliationReport> last_reports;
    kj::Vector<ManualInterventionItem> pending_interventions;
    Stats stats;
    bool running{false};
    bool strategy_frozen{false};
    int consecutive_mismatches{0};
    size_t max_event_history{1000};
    std::int64_t intervention_counter{0};
  };

  kj::MutexGuarded<ReconcilerState> guarded_;

  // Callbacks
  kj::Maybe<ReconciliationCallback> event_callback_;
  kj::Maybe<StrategyFreezeCallback> freeze_callback_;

  // Cancellation flag for the loop
  kj::Own<kj::PromiseFulfiller<void>> stop_fulfiller_;
  kj::Promise<void> stop_promise_;
};

// Convert enum to string for logging
kj::StringPtr to_string(ReconciliationEventType type);
kj::StringPtr to_string(ReconciliationAction action);
kj::StringPtr to_string(ReconciliationSeverity severity);

} // namespace veloz::exec
