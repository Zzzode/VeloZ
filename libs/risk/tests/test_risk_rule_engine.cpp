#include "kj/test.h"
#include "veloz/risk/risk_rule_engine.h"

namespace {

using namespace veloz::risk;
using namespace veloz::exec;

// Helper to create a simple order request
PlaceOrderRequest make_order(double qty, double price, OrderSide side = OrderSide::Buy) {
  PlaceOrderRequest req;
  req.symbol = veloz::common::SymbolId{"BTCUSDT"};
  req.qty = qty;
  req.price = price;
  req.side = side;
  req.type = OrderType::Limit;
  return req;
}

// Helper to create a simple rule
RiskRule make_rule(kj::StringPtr id, int priority, RuleConditionType type, ComparisonOp op,
                   double value, RuleAction action) {
  RiskRule rule;
  rule.id = kj::heapString(id);
  rule.name = kj::heapString(id);
  rule.priority = priority;
  rule.enabled = true;
  rule.condition.type = type;
  rule.condition.op = op;
  rule.condition.value = value;
  rule.action = action;
  rule.rejection_reason = kj::str("Rule ", id, " triggered");
  return rule;
}

// ============================================================================
// Basic Rule Management Tests
// ============================================================================

KJ_TEST("RiskRuleEngine: Empty engine allows all") {
  RiskRuleEngine engine;

  PlaceOrderRequest order = make_order(1.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;

  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);
  KJ_EXPECT(!result.matched);
}

KJ_TEST("RiskRuleEngine: Add and get rule") {
  RiskRuleEngine engine;

  RiskRule rule = make_rule("test-rule", 1, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            10.0, RuleAction::Reject);
  engine.add_rule(kj::mv(rule));

  KJ_EXPECT(engine.rule_count() == 1);

  const auto* retrieved = engine.get_rule("test-rule");
  KJ_ASSERT(retrieved != nullptr);
  KJ_EXPECT(retrieved->id == "test-rule"_kj);
  KJ_EXPECT(retrieved->priority == 1);
}

KJ_TEST("RiskRuleEngine: Update rule") {
  RiskRuleEngine engine;

  RiskRule rule = make_rule("test-rule", 1, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            10.0, RuleAction::Reject);
  engine.add_rule(kj::mv(rule));

  RiskRule updated = make_rule("test-rule", 2, RuleConditionType::OrderSize,
                               ComparisonOp::GreaterThan, 20.0, RuleAction::Warn);
  bool success = engine.update_rule("test-rule", kj::mv(updated));
  KJ_EXPECT(success);

  const auto* retrieved = engine.get_rule("test-rule");
  KJ_ASSERT(retrieved != nullptr);
  KJ_EXPECT(retrieved->priority == 2);
  KJ_EXPECT(retrieved->condition.value == 20.0);
  KJ_EXPECT(retrieved->action == RuleAction::Warn);
}

KJ_TEST("RiskRuleEngine: Remove rule") {
  RiskRuleEngine engine;

  engine.add_rule(make_rule("rule1", 1, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            10.0, RuleAction::Reject));
  engine.add_rule(make_rule("rule2", 2, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            20.0, RuleAction::Reject));

  KJ_EXPECT(engine.rule_count() == 2);

  bool success = engine.remove_rule("rule1");
  KJ_EXPECT(success);
  KJ_EXPECT(engine.rule_count() == 1);
  KJ_EXPECT(engine.get_rule("rule1") == nullptr);
  KJ_EXPECT(engine.get_rule("rule2") != nullptr);
}

KJ_TEST("RiskRuleEngine: Enable/disable rule") {
  RiskRuleEngine engine;

  RiskRule rule = make_rule("test-rule", 1, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            5.0, RuleAction::Reject);
  engine.add_rule(kj::mv(rule));

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;

  // Rule should trigger
  auto result1 = engine.evaluate(ctx);
  KJ_EXPECT(result1.action == RuleAction::Reject);

  // Disable rule
  engine.disable_rule("test-rule");
  auto result2 = engine.evaluate(ctx);
  KJ_EXPECT(result2.action == RuleAction::Allow);

  // Re-enable rule
  engine.enable_rule("test-rule");
  auto result3 = engine.evaluate(ctx);
  KJ_EXPECT(result3.action == RuleAction::Reject);
}

KJ_TEST("RiskRuleEngine: Clear rules") {
  RiskRuleEngine engine;

  engine.add_rule(make_rule("rule1", 1, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            10.0, RuleAction::Reject));
  engine.add_rule(make_rule("rule2", 2, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            20.0, RuleAction::Reject));

  KJ_EXPECT(engine.rule_count() == 2);

  engine.clear_rules();
  KJ_EXPECT(engine.rule_count() == 0);
}

// ============================================================================
// Priority and Evaluation Order Tests
// ============================================================================

KJ_TEST("RiskRuleEngine: Rules sorted by priority") {
  RiskRuleEngine engine;

  // Add rules in reverse priority order
  engine.add_rule(make_rule("low-priority", 100, RuleConditionType::OrderSize,
                            ComparisonOp::GreaterThan, 5.0, RuleAction::Warn));
  engine.add_rule(make_rule("high-priority", 1, RuleConditionType::OrderSize,
                            ComparisonOp::GreaterThan, 5.0, RuleAction::Reject));
  engine.add_rule(make_rule("mid-priority", 50, RuleConditionType::OrderSize,
                            ComparisonOp::GreaterThan, 5.0, RuleAction::RequireApproval));

  const auto& rules = engine.get_rules();
  KJ_EXPECT(rules[0].id == "high-priority"_kj);
  KJ_EXPECT(rules[1].id == "mid-priority"_kj);
  KJ_EXPECT(rules[2].id == "low-priority"_kj);
}

KJ_TEST("RiskRuleEngine: First matching rule wins") {
  RiskRuleEngine engine;

  // Higher priority rule (lower number) should win
  engine.add_rule(make_rule("reject-rule", 1, RuleConditionType::OrderSize,
                            ComparisonOp::GreaterThan, 5.0, RuleAction::Reject));
  engine.add_rule(make_rule("warn-rule", 2, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            5.0, RuleAction::Warn));

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;

  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);
  KJ_EXPECT(result.rule_id == "reject-rule"_kj);
}

// ============================================================================
// Comparison Operator Tests
// ============================================================================

KJ_TEST("RiskRuleEngine: ComparisonOp Equal") {
  RiskRuleEngine engine;
  engine.add_rule(make_rule("equal-rule", 1, RuleConditionType::OrderSize, ComparisonOp::Equal,
                            10.0, RuleAction::Reject));

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;

  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);

  order.qty = 10.1;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);
}

KJ_TEST("RiskRuleEngine: ComparisonOp NotEqual") {
  RiskRuleEngine engine;
  engine.add_rule(make_rule("not-equal-rule", 1, RuleConditionType::OrderSize,
                            ComparisonOp::NotEqual, 10.0, RuleAction::Reject));

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;

  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);

  order.qty = 5.0;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);
}

KJ_TEST("RiskRuleEngine: ComparisonOp GreaterThan") {
  RiskRuleEngine engine;
  engine.add_rule(make_rule("gt-rule", 1, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            10.0, RuleAction::Reject));

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;

  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow); // Not greater than

  order.qty = 10.1;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);
}

KJ_TEST("RiskRuleEngine: ComparisonOp LessThan") {
  RiskRuleEngine engine;
  engine.add_rule(make_rule("lt-rule", 1, RuleConditionType::OrderSize, ComparisonOp::LessThan, 1.0,
                            RuleAction::Reject));

  PlaceOrderRequest order = make_order(0.5, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;

  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);

  order.qty = 1.0;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);
}

KJ_TEST("RiskRuleEngine: ComparisonOp Between") {
  RiskRuleEngine engine;

  RiskRule rule;
  rule.id = kj::heapString("between-rule");
  rule.name = kj::heapString("between-rule");
  rule.priority = 1;
  rule.enabled = true;
  rule.condition.type = RuleConditionType::OrderSize;
  rule.condition.op = ComparisonOp::Between;
  rule.condition.value = 5.0;
  rule.condition.value2 = 15.0;
  rule.action = RuleAction::Reject;
  rule.rejection_reason = kj::heapString("Order size in restricted range");
  engine.add_rule(kj::mv(rule));

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;

  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);

  order.qty = 4.0;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);

  order.qty = 16.0;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);
}

// ============================================================================
// Condition Type Tests
// ============================================================================

KJ_TEST("RiskRuleEngine: OrderValue condition") {
  RiskRuleEngine engine;
  engine.add_rule(make_rule("value-rule", 1, RuleConditionType::OrderValue,
                            ComparisonOp::GreaterThan, 100000.0, RuleAction::Reject));

  PlaceOrderRequest order = make_order(1.0, 50000.0); // Value = 50000
  EvaluationContext ctx;
  ctx.order = &order;

  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);

  order.qty = 3.0; // Value = 150000
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);
}

KJ_TEST("RiskRuleEngine: OrderSide condition") {
  RiskRuleEngine engine;
  // Reject sell orders (side value = -1)
  engine.add_rule(make_rule("no-sell-rule", 1, RuleConditionType::OrderSide, ComparisonOp::Equal,
                            -1.0, RuleAction::Reject));

  PlaceOrderRequest buy_order = make_order(1.0, 50000.0, OrderSide::Buy);
  EvaluationContext ctx;
  ctx.order = &buy_order;

  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);

  PlaceOrderRequest sell_order = make_order(1.0, 50000.0, OrderSide::Sell);
  ctx.order = &sell_order;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);
}

KJ_TEST("RiskRuleEngine: AccountDrawdown condition") {
  RiskRuleEngine engine;
  engine.add_rule(make_rule("drawdown-rule", 1, RuleConditionType::AccountDrawdown,
                            ComparisonOp::GreaterThan, 0.10, RuleAction::Reject));

  PlaceOrderRequest order = make_order(1.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;
  ctx.account_drawdown = 0.05;

  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);

  ctx.account_drawdown = 0.15;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);
}

KJ_TEST("RiskRuleEngine: MarketVolatility condition") {
  RiskRuleEngine engine;
  engine.add_rule(make_rule("vol-rule", 1, RuleConditionType::MarketVolatility,
                            ComparisonOp::GreaterThan, 0.5, RuleAction::Reject));

  PlaceOrderRequest order = make_order(1.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;
  ctx.market_volatility = 0.3;

  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);

  ctx.market_volatility = 0.8;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);
}

KJ_TEST("RiskRuleEngine: TimeOfDay condition") {
  RiskRuleEngine engine;

  RiskRule rule;
  rule.id = kj::heapString("time-rule");
  rule.name = kj::heapString("time-rule");
  rule.priority = 1;
  rule.enabled = true;
  rule.condition.type = RuleConditionType::TimeOfDay;
  rule.condition.op = ComparisonOp::Between;
  rule.condition.value = 9.0;      // 9 AM
  rule.condition.value2 = 17.0;    // 5 PM
  rule.action = RuleAction::Allow; // Allow during trading hours
  rule.rejection_reason = kj::heapString("Trading hours only");
  engine.add_rule(kj::mv(rule));

  // Add reject rule for outside hours
  engine.add_rule(make_rule("reject-outside", 2, RuleConditionType::OrderSize,
                            ComparisonOp::GreaterOrEqual, 0.0, RuleAction::Reject));

  PlaceOrderRequest order = make_order(1.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;
  ctx.current_hour = 12; // Noon - within trading hours

  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);

  ctx.current_hour = 20; // 8 PM - outside trading hours
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);
}

// ============================================================================
// Composite Condition Tests
// ============================================================================

KJ_TEST("RiskRuleEngine: AND composite condition") {
  RiskRuleEngine engine;

  RiskRule rule;
  rule.id = kj::heapString("and-rule");
  rule.name = kj::heapString("and-rule");
  rule.priority = 1;
  rule.enabled = true;
  rule.condition.type = RuleConditionType::And;
  rule.action = RuleAction::Reject;
  rule.rejection_reason = kj::heapString("Both conditions met");

  // Child 1: Order size > 5
  RuleCondition child1;
  child1.type = RuleConditionType::OrderSize;
  child1.op = ComparisonOp::GreaterThan;
  child1.value = 5.0;
  rule.condition.children.add(kj::mv(child1));

  // Child 2: Market volatility > 0.5
  RuleCondition child2;
  child2.type = RuleConditionType::MarketVolatility;
  child2.op = ComparisonOp::GreaterThan;
  child2.value = 0.5;
  rule.condition.children.add(kj::mv(child2));

  engine.add_rule(kj::mv(rule));

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;
  ctx.market_volatility = 0.3;

  // Only one condition met - should not trigger
  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);

  // Both conditions met - should trigger
  ctx.market_volatility = 0.8;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);
}

KJ_TEST("RiskRuleEngine: OR composite condition") {
  RiskRuleEngine engine;

  RiskRule rule;
  rule.id = kj::heapString("or-rule");
  rule.name = kj::heapString("or-rule");
  rule.priority = 1;
  rule.enabled = true;
  rule.condition.type = RuleConditionType::Or;
  rule.action = RuleAction::Reject;
  rule.rejection_reason = kj::heapString("Either condition met");

  // Child 1: Order size > 100
  RuleCondition child1;
  child1.type = RuleConditionType::OrderSize;
  child1.op = ComparisonOp::GreaterThan;
  child1.value = 100.0;
  rule.condition.children.add(kj::mv(child1));

  // Child 2: Market volatility > 0.9
  RuleCondition child2;
  child2.type = RuleConditionType::MarketVolatility;
  child2.op = ComparisonOp::GreaterThan;
  child2.value = 0.9;
  rule.condition.children.add(kj::mv(child2));

  engine.add_rule(kj::mv(rule));

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;
  ctx.market_volatility = 0.3;

  // Neither condition met
  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);

  // First condition met
  order.qty = 150.0;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);

  // Second condition met
  order.qty = 10.0;
  ctx.market_volatility = 0.95;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);
}

KJ_TEST("RiskRuleEngine: NOT composite condition") {
  RiskRuleEngine engine;

  RiskRule rule;
  rule.id = kj::heapString("not-rule");
  rule.name = kj::heapString("not-rule");
  rule.priority = 1;
  rule.enabled = true;
  rule.condition.type = RuleConditionType::Not;
  rule.action = RuleAction::Reject;
  rule.rejection_reason = kj::heapString("Condition NOT met");

  // Child: Order size > 10 (NOT this = order size <= 10)
  RuleCondition child;
  child.type = RuleConditionType::OrderSize;
  child.op = ComparisonOp::GreaterThan;
  child.value = 10.0;
  rule.condition.children.add(kj::mv(child));

  engine.add_rule(kj::mv(rule));

  PlaceOrderRequest order = make_order(5.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;

  // Order size <= 10, so NOT(size > 10) = true -> reject
  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);

  // Order size > 10, so NOT(size > 10) = false -> allow
  order.qty = 15.0;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);
}

// Phase 6 Test: COMP-006 - Nested composite condition ((A AND B) OR C)
KJ_TEST("RiskRuleEngine: Nested composite condition (A AND B) OR C") {
  RiskRuleEngine engine;

  RiskRule rule;
  rule.id = kj::heapString("nested-rule");
  rule.name = kj::heapString("nested-rule");
  rule.priority = 1;
  rule.enabled = true;
  rule.condition.type = RuleConditionType::Or;
  rule.action = RuleAction::Reject;
  rule.rejection_reason = kj::heapString("Nested condition met");

  // First child: (A AND B) - OrderSize > 5 AND MarketVolatility > 0.5
  RuleCondition and_condition;
  and_condition.type = RuleConditionType::And;

  RuleCondition child_a;
  child_a.type = RuleConditionType::OrderSize;
  child_a.op = ComparisonOp::GreaterThan;
  child_a.value = 5.0;
  and_condition.children.add(kj::mv(child_a));

  RuleCondition child_b;
  child_b.type = RuleConditionType::MarketVolatility;
  child_b.op = ComparisonOp::GreaterThan;
  child_b.value = 0.5;
  and_condition.children.add(kj::mv(child_b));

  rule.condition.children.add(kj::mv(and_condition));

  // Second child: C - AccountDrawdown > 0.1
  RuleCondition child_c;
  child_c.type = RuleConditionType::AccountDrawdown;
  child_c.op = ComparisonOp::GreaterThan;
  child_c.value = 0.1;
  rule.condition.children.add(kj::mv(child_c));

  engine.add_rule(kj::mv(rule));

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;
  ctx.market_volatility = 0.3; // B is false
  ctx.account_drawdown = 0.05; // C is false

  // A=true, B=false, C=false -> (true AND false) OR false = false -> Allow
  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);

  // A=true, B=true, C=false -> (true AND true) OR false = true -> Reject
  ctx.market_volatility = 0.8;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);

  // A=false, B=true, C=true -> (false AND true) OR true = true -> Reject
  order.qty = 3.0;             // A is now false
  ctx.account_drawdown = 0.15; // C is true
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);

  // A=false, B=false, C=false -> (false AND false) OR false = false -> Allow
  ctx.market_volatility = 0.3;
  ctx.account_drawdown = 0.05;
  result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Allow);
}

// Phase 6 Test: PRI-003 - Same priority rules (first match wins)
KJ_TEST("RiskRuleEngine: Same priority rules - first match wins") {
  RiskRuleEngine engine;

  // Add two rules with same priority - first added should be evaluated first
  engine.add_rule(make_rule("rule-reject", 1, RuleConditionType::OrderSize,
                            ComparisonOp::GreaterThan, 5.0, RuleAction::Reject));
  engine.add_rule(make_rule("rule-warn", 1, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            5.0, RuleAction::Warn));

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;

  // Both rules match, but first one (Reject) should win
  auto result = engine.evaluate(ctx);
  KJ_EXPECT(result.action == RuleAction::Reject);
  KJ_EXPECT(result.rule_id == "rule-reject"_kj);
}

// ============================================================================
// Audit Log Tests
// ============================================================================

KJ_TEST("RiskRuleEngine: Audit log records evaluations") {
  RiskRuleEngine engine;
  engine.add_rule(make_rule("test-rule", 1, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            5.0, RuleAction::Reject));

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;

  (void)engine.evaluate(ctx);
  (void)engine.evaluate(ctx);
  (void)engine.evaluate(ctx);

  auto recent = engine.get_recent_evaluations(10);
  KJ_EXPECT(recent.size() == 3);
}

KJ_TEST("RiskRuleEngine: Audit log respects max size") {
  RiskRuleEngine engine;
  engine.set_max_audit_log_size(5);
  engine.add_rule(make_rule("test-rule", 1, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            5.0, RuleAction::Reject));

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;

  for (int i = 0; i < 10; ++i) {
    (void)engine.evaluate(ctx);
  }

  auto recent = engine.get_recent_evaluations(100);
  KJ_EXPECT(recent.size() == 5);
}

KJ_TEST("RiskRuleEngine: Audit callback called") {
  RiskRuleEngine engine;
  engine.add_rule(make_rule("test-rule", 1, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            5.0, RuleAction::Reject));

  int callback_count = 0;
  engine.set_audit_callback(
      [&callback_count](const EvaluationResult& result) { callback_count++; });

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;

  (void)engine.evaluate(ctx);
  (void)engine.evaluate(ctx);

  KJ_EXPECT(callback_count == 2);
}

KJ_TEST("RiskRuleEngine: Clear audit log") {
  RiskRuleEngine engine;
  engine.add_rule(make_rule("test-rule", 1, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            5.0, RuleAction::Reject));

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;

  (void)engine.evaluate(ctx);
  (void)engine.evaluate(ctx);

  auto recent = engine.get_recent_evaluations(10);
  KJ_EXPECT(recent.size() == 2);

  engine.clear_audit_log();
  recent = engine.get_recent_evaluations(10);
  KJ_EXPECT(recent.size() == 0);
}

// ============================================================================
// Evaluate All Tests
// ============================================================================

KJ_TEST("RiskRuleEngine: Evaluate all returns all rule results") {
  RiskRuleEngine engine;
  engine.add_rule(make_rule("rule1", 1, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            5.0, RuleAction::Reject));
  engine.add_rule(make_rule("rule2", 2, RuleConditionType::OrderSize, ComparisonOp::GreaterThan,
                            100.0, RuleAction::Warn));
  engine.add_rule(make_rule("rule3", 3, RuleConditionType::MarketVolatility,
                            ComparisonOp::GreaterThan, 0.5, RuleAction::RequireApproval));

  PlaceOrderRequest order = make_order(10.0, 50000.0);
  EvaluationContext ctx;
  ctx.order = &order;
  ctx.market_volatility = 0.3;

  auto results = engine.evaluate_all(ctx);
  KJ_EXPECT(results.size() == 3);

  // rule1 should match (10 > 5)
  KJ_EXPECT(results[0].matched);
  KJ_EXPECT(results[0].action == RuleAction::Reject);

  // rule2 should not match (10 not > 100)
  KJ_EXPECT(!results[1].matched);

  // rule3 should not match (0.3 not > 0.5)
  KJ_EXPECT(!results[2].matched);
}

// ============================================================================
// String Conversion Tests
// ============================================================================

KJ_TEST("rule_action_to_string: All actions") {
  KJ_EXPECT(rule_action_to_string(RuleAction::Allow) == "Allow"_kj);
  KJ_EXPECT(rule_action_to_string(RuleAction::Reject) == "Reject"_kj);
  KJ_EXPECT(rule_action_to_string(RuleAction::Warn) == "Warn"_kj);
  KJ_EXPECT(rule_action_to_string(RuleAction::RequireApproval) == "RequireApproval"_kj);
}

KJ_TEST("rule_condition_type_to_string: All types") {
  KJ_EXPECT(rule_condition_type_to_string(RuleConditionType::OrderSize) == "OrderSize"_kj);
  KJ_EXPECT(rule_condition_type_to_string(RuleConditionType::OrderValue) == "OrderValue"_kj);
  KJ_EXPECT(rule_condition_type_to_string(RuleConditionType::And) == "And"_kj);
  KJ_EXPECT(rule_condition_type_to_string(RuleConditionType::Or) == "Or"_kj);
  KJ_EXPECT(rule_condition_type_to_string(RuleConditionType::Not) == "Not"_kj);
}

KJ_TEST("comparison_op_to_string: All operators") {
  KJ_EXPECT(comparison_op_to_string(ComparisonOp::Equal) == "Equal"_kj);
  KJ_EXPECT(comparison_op_to_string(ComparisonOp::NotEqual) == "NotEqual"_kj);
  KJ_EXPECT(comparison_op_to_string(ComparisonOp::GreaterThan) == "GreaterThan"_kj);
  KJ_EXPECT(comparison_op_to_string(ComparisonOp::GreaterOrEqual) == "GreaterOrEqual"_kj);
  KJ_EXPECT(comparison_op_to_string(ComparisonOp::LessThan) == "LessThan"_kj);
  KJ_EXPECT(comparison_op_to_string(ComparisonOp::LessOrEqual) == "LessOrEqual"_kj);
  KJ_EXPECT(comparison_op_to_string(ComparisonOp::Between) == "Between"_kj);
}

} // namespace
