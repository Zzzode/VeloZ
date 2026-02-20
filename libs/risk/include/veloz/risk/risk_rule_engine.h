#pragma once

#include "veloz/exec/order_api.h"
#include "veloz/oms/position.h"

#include <cstdint>
#include <kj/common.h>
#include <kj/function.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::risk {

/**
 * @brief Rule action enumeration
 *
 * Defines the action to take when a rule matches.
 */
enum class RuleAction : std::uint8_t {
  Allow = 0,           ///< Allow the order to proceed
  Reject = 1,          ///< Reject the order
  Warn = 2,            ///< Allow but emit warning
  RequireApproval = 3, ///< Require manual approval
};

/**
 * @brief Rule condition type enumeration
 *
 * Defines the types of conditions that can be evaluated.
 */
enum class RuleConditionType : std::uint8_t {
  // Order conditions
  OrderSize = 0,
  OrderValue = 1,
  OrderPrice = 2,
  OrderSide = 3,

  // Position conditions
  PositionSize = 4,
  PositionValue = 5,
  PositionPnL = 6,

  // Account conditions
  AccountExposure = 7,
  AccountDrawdown = 8,
  AccountLeverage = 9,

  // Market conditions
  MarketVolatility = 10,
  MarketSpread = 11,

  // Time conditions
  TimeOfDay = 12,
  DayOfWeek = 13,

  // Composite conditions
  And = 100,
  Or = 101,
  Not = 102,
};

/**
 * @brief Comparison operator enumeration
 */
enum class ComparisonOp : std::uint8_t {
  Equal = 0,
  NotEqual = 1,
  GreaterThan = 2,
  GreaterOrEqual = 3,
  LessThan = 4,
  LessOrEqual = 5,
  Between = 6,
};

/**
 * @brief Rule condition structure
 *
 * Represents a single condition or composite condition in a rule.
 */
struct RuleCondition {
  RuleConditionType type{RuleConditionType::OrderSize};
  ComparisonOp op{ComparisonOp::LessThan};
  double value{0.0};
  double value2{0.0}; ///< For Between operator
  kj::String symbol;  ///< Optional: symbol-specific condition

  // For composite conditions (And, Or, Not)
  kj::Vector<RuleCondition> children;

  RuleCondition() = default;
  RuleCondition(RuleCondition&&) = default;
  RuleCondition& operator=(RuleCondition&&) = default;

  // Copy constructor for deep copy
  RuleCondition(const RuleCondition& other);
  RuleCondition& operator=(const RuleCondition& other);
};

/**
 * @brief Risk rule definition
 */
struct RiskRule {
  kj::String id;
  kj::String name;
  kj::String description;
  int priority{100}; ///< Lower = higher priority, evaluated first
  bool enabled{true};

  RuleCondition condition;
  RuleAction action{RuleAction::Reject};
  kj::String rejection_reason;

  // Metadata
  int64_t created_at_ns{0};
  int64_t updated_at_ns{0};
  kj::String created_by;

  RiskRule() = default;
  RiskRule(RiskRule&&) = default;
  RiskRule& operator=(RiskRule&&) = default;

  // Copy constructor for deep copy
  RiskRule(const RiskRule& other);
  RiskRule& operator=(const RiskRule& other);
};

/**
 * @brief Evaluation context for rule engine
 *
 * Contains all the data needed to evaluate rules.
 */
struct EvaluationContext {
  const veloz::exec::PlaceOrderRequest* order{nullptr};
  const veloz::oms::Position* position{nullptr};
  double account_equity{0.0};
  double account_drawdown{0.0};
  double account_leverage{0.0};
  double market_volatility{0.0};
  double market_spread{0.0};
  int64_t current_time_ns{0};
  int current_hour{0}; ///< Hour of day (0-23)
  int current_day{0};  ///< Day of week (0=Sunday, 6=Saturday)
};

/**
 * @brief Evaluation result from rule engine
 */
struct EvaluationResult {
  RuleAction action{RuleAction::Allow};
  kj::String rule_id;
  kj::String rule_name;
  kj::String reason;
  int64_t evaluation_time_ns{0};
  bool matched{false}; ///< Whether a rule matched
};

/**
 * @brief Risk rule engine
 *
 * Evaluates declarative risk rules against trading requests.
 * Supports composable conditions, priority-based evaluation,
 * and audit logging.
 */
class RiskRuleEngine final {
public:
  RiskRuleEngine() = default;

  // Rule management
  /**
   * @brief Add a rule to the engine
   * @param rule Rule to add (ownership transferred)
   */
  void add_rule(RiskRule rule);

  /**
   * @brief Update an existing rule
   * @param rule_id ID of rule to update
   * @param rule New rule definition
   * @return True if rule was found and updated
   */
  bool update_rule(kj::StringPtr rule_id, RiskRule rule);

  /**
   * @brief Remove a rule from the engine
   * @param rule_id ID of rule to remove
   * @return True if rule was found and removed
   */
  bool remove_rule(kj::StringPtr rule_id);

  /**
   * @brief Enable a rule
   * @param rule_id ID of rule to enable
   * @return True if rule was found
   */
  bool enable_rule(kj::StringPtr rule_id);

  /**
   * @brief Disable a rule
   * @param rule_id ID of rule to disable
   * @return True if rule was found
   */
  bool disable_rule(kj::StringPtr rule_id);

  /**
   * @brief Get a rule by ID
   * @param rule_id ID of rule to get
   * @return Pointer to rule, or nullptr if not found
   */
  [[nodiscard]] const RiskRule* get_rule(kj::StringPtr rule_id) const;

  /**
   * @brief Get all rules
   * @return Vector of all rules
   */
  [[nodiscard]] const kj::Vector<RiskRule>& get_rules() const;

  /**
   * @brief Get number of rules
   * @return Number of rules
   */
  [[nodiscard]] size_t rule_count() const;

  /**
   * @brief Clear all rules
   */
  void clear_rules();

  // Evaluation
  /**
   * @brief Evaluate rules against context
   *
   * Short-circuit evaluation: first matching rule wins.
   * Rules are evaluated in priority order (lower = higher priority).
   * Returns Allow if no rules match.
   *
   * @param ctx Evaluation context
   * @return Evaluation result
   */
  [[nodiscard]] EvaluationResult evaluate(const EvaluationContext& ctx) const;

  /**
   * @brief Evaluate all rules (for debugging/audit)
   *
   * Evaluates all rules and returns results for each.
   *
   * @param ctx Evaluation context
   * @return Vector of evaluation results
   */
  [[nodiscard]] kj::Vector<EvaluationResult> evaluate_all(const EvaluationContext& ctx) const;

  // Audit
  /**
   * @brief Get recent evaluation results
   * @param count Maximum number of results to return
   * @return Vector of recent evaluation results
   */
  [[nodiscard]] kj::Vector<EvaluationResult> get_recent_evaluations(int count) const;

  /**
   * @brief Set audit callback
   *
   * Called after each evaluation with the result.
   *
   * @param callback Audit callback function
   */
  void set_audit_callback(kj::Function<void(const EvaluationResult&)> callback);

  /**
   * @brief Clear audit log
   */
  void clear_audit_log();

  /**
   * @brief Set maximum audit log size
   * @param max_size Maximum number of entries to keep
   */
  void set_max_audit_log_size(size_t max_size);

private:
  /**
   * @brief Evaluate a single condition
   * @param cond Condition to evaluate
   * @param ctx Evaluation context
   * @return True if condition matches
   */
  [[nodiscard]] bool evaluate_condition(const RuleCondition& cond,
                                        const EvaluationContext& ctx) const;

  /**
   * @brief Compare a value using the specified operator
   * @param actual Actual value
   * @param op Comparison operator
   * @param expected Expected value
   * @param expected2 Second expected value (for Between)
   * @return True if comparison succeeds
   */
  [[nodiscard]] bool compare_value(double actual, ComparisonOp op, double expected,
                                   double expected2) const;

  /**
   * @brief Sort rules by priority
   */
  void sort_rules();

  /**
   * @brief Record evaluation result in audit log
   * @param result Evaluation result
   */
  void record_evaluation(const EvaluationResult& result) const;

  kj::Vector<RiskRule> rules_; ///< Sorted by priority
  mutable kj::Vector<EvaluationResult> audit_log_;
  mutable kj::Maybe<kj::Function<void(const EvaluationResult&)>> audit_callback_;
  size_t max_audit_log_size_{1000};
};

/**
 * @brief Convert RuleAction to string
 * @param action Rule action
 * @return String representation
 */
[[nodiscard]] kj::StringPtr rule_action_to_string(RuleAction action);

/**
 * @brief Convert RuleConditionType to string
 * @param type Condition type
 * @return String representation
 */
[[nodiscard]] kj::StringPtr rule_condition_type_to_string(RuleConditionType type);

/**
 * @brief Convert ComparisonOp to string
 * @param op Comparison operator
 * @return String representation
 */
[[nodiscard]] kj::StringPtr comparison_op_to_string(ComparisonOp op);

} // namespace veloz::risk
