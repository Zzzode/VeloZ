#include "veloz/risk/risk_rule_engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace veloz::risk {

namespace {
int64_t current_timestamp_ns() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}
} // namespace

// ============================================================================
// RuleCondition Implementation
// ============================================================================

RuleCondition::RuleCondition(const RuleCondition& other)
    : type(other.type), op(other.op), value(other.value), value2(other.value2),
      symbol(kj::heapString(other.symbol)) {
  for (const auto& child : other.children) {
    children.add(RuleCondition(child));
  }
}

RuleCondition& RuleCondition::operator=(const RuleCondition& other) {
  if (this != &other) {
    type = other.type;
    op = other.op;
    value = other.value;
    value2 = other.value2;
    symbol = kj::heapString(other.symbol);
    children.clear();
    for (const auto& child : other.children) {
      children.add(RuleCondition(child));
    }
  }
  return *this;
}

// ============================================================================
// RiskRule Implementation
// ============================================================================

RiskRule::RiskRule(const RiskRule& other)
    : id(kj::heapString(other.id)), name(kj::heapString(other.name)),
      description(kj::heapString(other.description)), priority(other.priority),
      enabled(other.enabled), condition(other.condition), action(other.action),
      rejection_reason(kj::heapString(other.rejection_reason)), created_at_ns(other.created_at_ns),
      updated_at_ns(other.updated_at_ns), created_by(kj::heapString(other.created_by)) {}

RiskRule& RiskRule::operator=(const RiskRule& other) {
  if (this != &other) {
    id = kj::heapString(other.id);
    name = kj::heapString(other.name);
    description = kj::heapString(other.description);
    priority = other.priority;
    enabled = other.enabled;
    condition = other.condition;
    action = other.action;
    rejection_reason = kj::heapString(other.rejection_reason);
    created_at_ns = other.created_at_ns;
    updated_at_ns = other.updated_at_ns;
    created_by = kj::heapString(other.created_by);
  }
  return *this;
}

// ============================================================================
// RiskRuleEngine Implementation
// ============================================================================

void RiskRuleEngine::add_rule(RiskRule rule) {
  rules_.add(kj::mv(rule));
  sort_rules();
}

bool RiskRuleEngine::update_rule(kj::StringPtr rule_id, RiskRule rule) {
  for (size_t i = 0; i < rules_.size(); ++i) {
    if (rules_[i].id == rule_id) {
      rule.updated_at_ns = current_timestamp_ns();
      rules_[i] = kj::mv(rule);
      sort_rules();
      return true;
    }
  }
  return false;
}

bool RiskRuleEngine::remove_rule(kj::StringPtr rule_id) {
  for (size_t i = 0; i < rules_.size(); ++i) {
    if (rules_[i].id == rule_id) {
      // Shift remaining elements
      kj::Vector<RiskRule> new_rules;
      for (size_t j = 0; j < rules_.size(); ++j) {
        if (j != i) {
          new_rules.add(kj::mv(rules_[j]));
        }
      }
      rules_ = kj::mv(new_rules);
      return true;
    }
  }
  return false;
}

bool RiskRuleEngine::enable_rule(kj::StringPtr rule_id) {
  for (auto& rule : rules_) {
    if (rule.id == rule_id) {
      rule.enabled = true;
      rule.updated_at_ns = current_timestamp_ns();
      return true;
    }
  }
  return false;
}

bool RiskRuleEngine::disable_rule(kj::StringPtr rule_id) {
  for (auto& rule : rules_) {
    if (rule.id == rule_id) {
      rule.enabled = false;
      rule.updated_at_ns = current_timestamp_ns();
      return true;
    }
  }
  return false;
}

const RiskRule* RiskRuleEngine::get_rule(kj::StringPtr rule_id) const {
  for (const auto& rule : rules_) {
    if (rule.id == rule_id) {
      return &rule;
    }
  }
  return nullptr;
}

const kj::Vector<RiskRule>& RiskRuleEngine::get_rules() const {
  return rules_;
}

size_t RiskRuleEngine::rule_count() const {
  return rules_.size();
}

void RiskRuleEngine::clear_rules() {
  rules_.clear();
}

EvaluationResult RiskRuleEngine::evaluate(const EvaluationContext& ctx) const {
  EvaluationResult result;
  result.action = RuleAction::Allow;
  result.evaluation_time_ns = current_timestamp_ns();
  result.matched = false;

  for (const auto& rule : rules_) {
    if (!rule.enabled) {
      continue;
    }

    if (evaluate_condition(rule.condition, ctx)) {
      result.action = rule.action;
      result.rule_id = kj::heapString(rule.id);
      result.rule_name = kj::heapString(rule.name);
      result.reason = kj::heapString(rule.rejection_reason);
      result.matched = true;

      record_evaluation(result);
      return result;
    }
  }

  // No rule matched - default allow
  result.reason = kj::heapString("No rules matched");
  record_evaluation(result);
  return result;
}

kj::Vector<EvaluationResult> RiskRuleEngine::evaluate_all(const EvaluationContext& ctx) const {
  kj::Vector<EvaluationResult> results;

  for (const auto& rule : rules_) {
    EvaluationResult result;
    result.evaluation_time_ns = current_timestamp_ns();
    result.rule_id = kj::heapString(rule.id);
    result.rule_name = kj::heapString(rule.name);

    if (!rule.enabled) {
      result.action = RuleAction::Allow;
      result.reason = kj::heapString("Rule disabled");
      result.matched = false;
    } else if (evaluate_condition(rule.condition, ctx)) {
      result.action = rule.action;
      result.reason = kj::heapString(rule.rejection_reason);
      result.matched = true;
    } else {
      result.action = RuleAction::Allow;
      result.reason = kj::heapString("Condition not met");
      result.matched = false;
    }

    results.add(kj::mv(result));
  }

  return results;
}

kj::Vector<EvaluationResult> RiskRuleEngine::get_recent_evaluations(int count) const {
  kj::Vector<EvaluationResult> results;

  size_t start = 0;
  if (audit_log_.size() > static_cast<size_t>(count)) {
    start = audit_log_.size() - count;
  }

  for (size_t i = start; i < audit_log_.size(); ++i) {
    EvaluationResult copy;
    copy.action = audit_log_[i].action;
    copy.rule_id = kj::heapString(audit_log_[i].rule_id);
    copy.rule_name = kj::heapString(audit_log_[i].rule_name);
    copy.reason = kj::heapString(audit_log_[i].reason);
    copy.evaluation_time_ns = audit_log_[i].evaluation_time_ns;
    copy.matched = audit_log_[i].matched;
    results.add(kj::mv(copy));
  }

  return results;
}

void RiskRuleEngine::set_audit_callback(kj::Function<void(const EvaluationResult&)> callback) {
  audit_callback_ = kj::mv(callback);
}

void RiskRuleEngine::clear_audit_log() {
  audit_log_.clear();
}

void RiskRuleEngine::set_max_audit_log_size(size_t max_size) {
  max_audit_log_size_ = max_size;

  // Trim if necessary
  while (audit_log_.size() > max_audit_log_size_) {
    kj::Vector<EvaluationResult> trimmed;
    for (size_t i = 1; i < audit_log_.size(); ++i) {
      trimmed.add(kj::mv(audit_log_[i]));
    }
    audit_log_ = kj::mv(trimmed);
  }
}

bool RiskRuleEngine::evaluate_condition(const RuleCondition& cond,
                                        const EvaluationContext& ctx) const {
  switch (cond.type) {
  // Order conditions
  case RuleConditionType::OrderSize: {
    if (!ctx.order)
      return false;
    return compare_value(ctx.order->qty, cond.op, cond.value, cond.value2);
  }

  case RuleConditionType::OrderValue: {
    if (!ctx.order)
      return false;
    double price = 0.0;
    KJ_IF_SOME(p, ctx.order->price) {
      price = p;
    }
    double value = ctx.order->qty * price;
    return compare_value(value, cond.op, cond.value, cond.value2);
  }

  case RuleConditionType::OrderPrice: {
    if (!ctx.order)
      return false;
    KJ_IF_SOME(price, ctx.order->price) {
      return compare_value(price, cond.op, cond.value, cond.value2);
    }
    return false;
  }

  case RuleConditionType::OrderSide: {
    if (!ctx.order)
      return false;
    double side_value = (ctx.order->side == veloz::exec::OrderSide::Buy) ? 1.0 : -1.0;
    return compare_value(side_value, cond.op, cond.value, cond.value2);
  }

  // Position conditions
  case RuleConditionType::PositionSize: {
    if (!ctx.position)
      return false;
    return compare_value(std::abs(ctx.position->size()), cond.op, cond.value, cond.value2);
  }

  case RuleConditionType::PositionValue: {
    if (!ctx.position)
      return false;
    double value = std::abs(ctx.position->size()) * ctx.position->avg_price();
    return compare_value(value, cond.op, cond.value, cond.value2);
  }

  case RuleConditionType::PositionPnL: {
    if (!ctx.position)
      return false;
    return compare_value(ctx.position->realized_pnl(), cond.op, cond.value, cond.value2);
  }

  // Account conditions
  case RuleConditionType::AccountExposure: {
    return compare_value(ctx.account_equity, cond.op, cond.value, cond.value2);
  }

  case RuleConditionType::AccountDrawdown: {
    return compare_value(ctx.account_drawdown, cond.op, cond.value, cond.value2);
  }

  case RuleConditionType::AccountLeverage: {
    return compare_value(ctx.account_leverage, cond.op, cond.value, cond.value2);
  }

  // Market conditions
  case RuleConditionType::MarketVolatility: {
    return compare_value(ctx.market_volatility, cond.op, cond.value, cond.value2);
  }

  case RuleConditionType::MarketSpread: {
    return compare_value(ctx.market_spread, cond.op, cond.value, cond.value2);
  }

  // Time conditions
  case RuleConditionType::TimeOfDay: {
    return compare_value(static_cast<double>(ctx.current_hour), cond.op, cond.value, cond.value2);
  }

  case RuleConditionType::DayOfWeek: {
    return compare_value(static_cast<double>(ctx.current_day), cond.op, cond.value, cond.value2);
  }

  // Composite conditions
  case RuleConditionType::And: {
    for (const auto& child : cond.children) {
      if (!evaluate_condition(child, ctx)) {
        return false;
      }
    }
    return !cond.children.empty();
  }

  case RuleConditionType::Or: {
    for (const auto& child : cond.children) {
      if (evaluate_condition(child, ctx)) {
        return true;
      }
    }
    return false;
  }

  case RuleConditionType::Not: {
    if (cond.children.empty()) {
      return false;
    }
    return !evaluate_condition(cond.children[0], ctx);
  }
  }

  return false;
}

bool RiskRuleEngine::compare_value(double actual, ComparisonOp op, double expected,
                                   double expected2) const {
  constexpr double epsilon = 1e-9;

  switch (op) {
  case ComparisonOp::Equal:
    return std::abs(actual - expected) < epsilon;

  case ComparisonOp::NotEqual:
    return std::abs(actual - expected) >= epsilon;

  case ComparisonOp::GreaterThan:
    return actual > expected;

  case ComparisonOp::GreaterOrEqual:
    return actual >= expected - epsilon;

  case ComparisonOp::LessThan:
    return actual < expected;

  case ComparisonOp::LessOrEqual:
    return actual <= expected + epsilon;

  case ComparisonOp::Between:
    return actual >= expected - epsilon && actual <= expected2 + epsilon;
  }

  return false;
}

void RiskRuleEngine::sort_rules() {
  // Sort by priority (lower = higher priority)
  std::sort(rules_.begin(), rules_.end(),
            [](const RiskRule& a, const RiskRule& b) { return a.priority < b.priority; });
}

void RiskRuleEngine::record_evaluation(const EvaluationResult& result) const {
  // Add to audit log
  EvaluationResult copy;
  copy.action = result.action;
  copy.rule_id = kj::heapString(result.rule_id);
  copy.rule_name = kj::heapString(result.rule_name);
  copy.reason = kj::heapString(result.reason);
  copy.evaluation_time_ns = result.evaluation_time_ns;
  copy.matched = result.matched;

  audit_log_.add(kj::mv(copy));

  // Trim if necessary
  while (audit_log_.size() > max_audit_log_size_) {
    kj::Vector<EvaluationResult> trimmed;
    for (size_t i = 1; i < audit_log_.size(); ++i) {
      EvaluationResult entry;
      entry.action = audit_log_[i].action;
      entry.rule_id = kj::heapString(audit_log_[i].rule_id);
      entry.rule_name = kj::heapString(audit_log_[i].rule_name);
      entry.reason = kj::heapString(audit_log_[i].reason);
      entry.evaluation_time_ns = audit_log_[i].evaluation_time_ns;
      entry.matched = audit_log_[i].matched;
      trimmed.add(kj::mv(entry));
    }
    audit_log_ = kj::mv(trimmed);
  }

  // Call audit callback if set
  KJ_IF_SOME(callback, audit_callback_) {
    callback(result);
  }
}

// ============================================================================
// String Conversion Functions
// ============================================================================

kj::StringPtr rule_action_to_string(RuleAction action) {
  switch (action) {
  case RuleAction::Allow:
    return "Allow"_kj;
  case RuleAction::Reject:
    return "Reject"_kj;
  case RuleAction::Warn:
    return "Warn"_kj;
  case RuleAction::RequireApproval:
    return "RequireApproval"_kj;
  }
  return "Unknown"_kj;
}

kj::StringPtr rule_condition_type_to_string(RuleConditionType type) {
  switch (type) {
  case RuleConditionType::OrderSize:
    return "OrderSize"_kj;
  case RuleConditionType::OrderValue:
    return "OrderValue"_kj;
  case RuleConditionType::OrderPrice:
    return "OrderPrice"_kj;
  case RuleConditionType::OrderSide:
    return "OrderSide"_kj;
  case RuleConditionType::PositionSize:
    return "PositionSize"_kj;
  case RuleConditionType::PositionValue:
    return "PositionValue"_kj;
  case RuleConditionType::PositionPnL:
    return "PositionPnL"_kj;
  case RuleConditionType::AccountExposure:
    return "AccountExposure"_kj;
  case RuleConditionType::AccountDrawdown:
    return "AccountDrawdown"_kj;
  case RuleConditionType::AccountLeverage:
    return "AccountLeverage"_kj;
  case RuleConditionType::MarketVolatility:
    return "MarketVolatility"_kj;
  case RuleConditionType::MarketSpread:
    return "MarketSpread"_kj;
  case RuleConditionType::TimeOfDay:
    return "TimeOfDay"_kj;
  case RuleConditionType::DayOfWeek:
    return "DayOfWeek"_kj;
  case RuleConditionType::And:
    return "And"_kj;
  case RuleConditionType::Or:
    return "Or"_kj;
  case RuleConditionType::Not:
    return "Not"_kj;
  }
  return "Unknown"_kj;
}

kj::StringPtr comparison_op_to_string(ComparisonOp op) {
  switch (op) {
  case ComparisonOp::Equal:
    return "Equal"_kj;
  case ComparisonOp::NotEqual:
    return "NotEqual"_kj;
  case ComparisonOp::GreaterThan:
    return "GreaterThan"_kj;
  case ComparisonOp::GreaterOrEqual:
    return "GreaterOrEqual"_kj;
  case ComparisonOp::LessThan:
    return "LessThan"_kj;
  case ComparisonOp::LessOrEqual:
    return "LessOrEqual"_kj;
  case ComparisonOp::Between:
    return "Between"_kj;
  }
  return "Unknown"_kj;
}

} // namespace veloz::risk
