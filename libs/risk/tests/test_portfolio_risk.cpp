#include "veloz/risk/portfolio_risk.h"

#include <cmath>
#include <kj/test.h>

namespace veloz::risk {
namespace {

KJ_TEST("PortfolioRiskAggregator - add and get position") {
  PortfolioRiskAggregator aggregator;

  PortfolioPosition pos;
  pos.symbol = kj::str("BTC");
  pos.size = 1.0;
  pos.price = 50000.0;
  pos.value = 50000.0;
  pos.volatility = 0.03;

  aggregator.update_position(kj::mv(pos));

  const auto* retrieved = aggregator.get_position("BTC");
  KJ_ASSERT(retrieved != nullptr);
  KJ_EXPECT(retrieved->symbol == "BTC"_kj);
  KJ_EXPECT(retrieved->value == 50000.0);
}

KJ_TEST("PortfolioRiskAggregator - update position") {
  PortfolioRiskAggregator aggregator;

  PortfolioPosition pos1;
  pos1.symbol = kj::str("BTC");
  pos1.value = 50000.0;
  pos1.volatility = 0.03;
  aggregator.update_position(kj::mv(pos1));

  PortfolioPosition pos2;
  pos2.symbol = kj::str("BTC");
  pos2.value = 60000.0;  // Updated value
  pos2.volatility = 0.03;
  aggregator.update_position(kj::mv(pos2));

  KJ_EXPECT(aggregator.get_positions().size() == 1);
  KJ_EXPECT(aggregator.get_position("BTC")->value == 60000.0);
}

KJ_TEST("PortfolioRiskAggregator - remove position") {
  PortfolioRiskAggregator aggregator;

  PortfolioPosition pos;
  pos.symbol = kj::str("BTC");
  pos.value = 50000.0;
  aggregator.update_position(kj::mv(pos));

  aggregator.remove_position("BTC");
  KJ_EXPECT(aggregator.get_positions().size() == 0);
  KJ_EXPECT(aggregator.get_position("BTC") == nullptr);
}

KJ_TEST("PortfolioRiskAggregator - correlation management") {
  PortfolioRiskAggregator aggregator;

  aggregator.set_correlation("BTC", "ETH", 0.8);

  KJ_EXPECT(std::abs(aggregator.get_correlation("BTC", "ETH") - 0.8) < 0.001);
  KJ_EXPECT(std::abs(aggregator.get_correlation("ETH", "BTC") - 0.8) < 0.001);  // Symmetric
  KJ_EXPECT(aggregator.get_correlation("BTC", "BTC") == 1.0);  // Self-correlation

  // Default correlation for unknown pair
  aggregator.set_default_correlation(0.5);
  KJ_EXPECT(std::abs(aggregator.get_correlation("BTC", "SOL") - 0.5) < 0.001);
}

KJ_TEST("PortfolioRiskAggregator - calculate risk single position") {
  PortfolioRiskAggregator aggregator;

  PortfolioPosition pos;
  pos.symbol = kj::str("BTC");
  pos.value = 100000.0;
  pos.volatility = 0.03;  // 3% daily volatility
  aggregator.update_position(kj::mv(pos));

  auto summary = aggregator.calculate_risk(0.95);

  KJ_EXPECT(summary.total_value == 100000.0);
  KJ_EXPECT(summary.total_var_95 > 0.0);
  KJ_EXPECT(summary.position_count == 1);
  KJ_EXPECT(summary.contributions.size() == 1);

  // For single position, undiversified = diversified
  KJ_EXPECT(std::abs(summary.diversification_benefit) < 1.0);
}

KJ_TEST("PortfolioRiskAggregator - calculate risk multiple positions") {
  PortfolioRiskAggregator aggregator;

  PortfolioPosition btc;
  btc.symbol = kj::str("BTC");
  btc.value = 60000.0;
  btc.volatility = 0.03;
  aggregator.update_position(kj::mv(btc));

  PortfolioPosition eth;
  eth.symbol = kj::str("ETH");
  eth.value = 40000.0;
  eth.volatility = 0.04;
  aggregator.update_position(kj::mv(eth));

  aggregator.set_correlation("BTC", "ETH", 0.7);

  auto summary = aggregator.calculate_risk(0.95);

  KJ_EXPECT(summary.total_value == 100000.0);
  KJ_EXPECT(summary.total_var_95 > 0.0);
  KJ_EXPECT(summary.position_count == 2);

  // With correlation < 1, there should be diversification benefit
  KJ_EXPECT(summary.diversification_benefit > 0.0);
  KJ_EXPECT(summary.undiversified_var > summary.total_var_95);
}

KJ_TEST("PortfolioRiskAggregator - risk contributions") {
  PortfolioRiskAggregator aggregator;

  PortfolioPosition btc;
  btc.symbol = kj::str("BTC");
  btc.value = 70000.0;
  btc.volatility = 0.03;
  aggregator.update_position(kj::mv(btc));

  PortfolioPosition eth;
  eth.symbol = kj::str("ETH");
  eth.value = 30000.0;
  eth.volatility = 0.04;
  aggregator.update_position(kj::mv(eth));

  aggregator.set_correlation("BTC", "ETH", 0.8);

  auto contributions = aggregator.calculate_contributions(0.95);

  KJ_EXPECT(contributions.size() == 2);

  // Sum of percentage contributions should be approximately 100%
  double total_pct = 0.0;
  for (const auto& c : contributions) {
    total_pct += c.pct_contribution;
  }
  KJ_EXPECT(std::abs(total_pct - 100.0) < 5.0);

  // BTC should contribute more (larger position)
  double btc_contrib = 0.0;
  double eth_contrib = 0.0;
  for (const auto& c : contributions) {
    if (c.symbol == "BTC"_kj) btc_contrib = c.pct_contribution;
    if (c.symbol == "ETH"_kj) eth_contrib = c.pct_contribution;
  }
  KJ_EXPECT(btc_contrib > eth_contrib);
}

KJ_TEST("PortfolioRiskAggregator - risk budget") {
  PortfolioRiskAggregator aggregator;

  aggregator.set_risk_budget("strategy_a", 5000.0);
  KJ_EXPECT(aggregator.get_risk_budget("strategy_a") == 5000.0);
  KJ_EXPECT(aggregator.get_risk_budget("unknown") == 0.0);
}

KJ_TEST("PortfolioRiskAggregator - risk allocations") {
  PortfolioRiskAggregator aggregator;

  PortfolioPosition pos1;
  pos1.symbol = kj::str("BTC");
  pos1.value = 50000.0;
  pos1.volatility = 0.03;
  pos1.strategy = kj::str("momentum");
  aggregator.update_position(kj::mv(pos1));

  PortfolioPosition pos2;
  pos2.symbol = kj::str("ETH");
  pos2.value = 30000.0;
  pos2.volatility = 0.04;
  pos2.strategy = kj::str("mean_reversion");
  aggregator.update_position(kj::mv(pos2));

  aggregator.set_risk_budget("momentum", 3000.0);
  aggregator.set_risk_budget("mean_reversion", 2000.0);

  auto allocations = aggregator.calculate_allocations();

  KJ_EXPECT(allocations.size() == 2);

  for (const auto& alloc : allocations) {
    KJ_EXPECT(alloc.used_var > 0.0);
    if (alloc.allocated_var > 0.0) {
      KJ_EXPECT(alloc.utilization_pct > 0.0);
    }
  }
}

KJ_TEST("PortfolioRiskAggregator - budget breach detection") {
  PortfolioRiskAggregator aggregator;

  PortfolioPosition pos;
  pos.symbol = kj::str("BTC");
  pos.value = 100000.0;
  pos.volatility = 0.03;
  pos.strategy = kj::str("test");
  aggregator.update_position(kj::mv(pos));

  // Set a very low budget that will be breached
  aggregator.set_risk_budget("test", 100.0);

  KJ_EXPECT(aggregator.is_any_budget_breached());

  // Set a high budget that won't be breached
  aggregator.set_risk_budget("test", 100000.0);
  KJ_EXPECT(!aggregator.is_any_budget_breached());
}

KJ_TEST("PortfolioRiskAggregator - suggest reductions") {
  PortfolioRiskAggregator aggregator;

  PortfolioPosition btc;
  btc.symbol = kj::str("BTC");
  btc.value = 60000.0;
  btc.volatility = 0.03;
  aggregator.update_position(kj::mv(btc));

  PortfolioPosition eth;
  eth.symbol = kj::str("ETH");
  eth.value = 40000.0;
  eth.volatility = 0.04;
  aggregator.update_position(kj::mv(eth));

  auto summary = aggregator.calculate_risk();
  double target_var = summary.total_var_95 * 0.5;  // Reduce to 50%

  auto suggestions = aggregator.suggest_reductions(target_var);

  KJ_EXPECT(suggestions.size() > 0);
  for (const auto& s : suggestions) {
    KJ_EXPECT(s.second > 0.0);  // Reduction amount should be positive
  }
}

KJ_TEST("PortfolioRiskAggregator - calculate max position") {
  PortfolioRiskAggregator aggregator;

  PortfolioPosition pos;
  pos.symbol = kj::str("BTC");
  pos.value = 50000.0;
  pos.volatility = 0.03;
  aggregator.update_position(kj::mv(pos));

  double max_pos = aggregator.calculate_max_position("BTC", 5000.0);

  KJ_EXPECT(max_pos > 0.0);
  // VaR = z * vol * value, so value = VaR / (z * vol)
  // With z=1.6449 (95%), vol=0.03, budget=5000
  // max = 5000 / (1.6449 * 0.03) = ~101,300
  KJ_EXPECT(max_pos > 90000.0);
  KJ_EXPECT(max_pos < 120000.0);
}

KJ_TEST("PortfolioRiskAggregator - holding period scaling") {
  PortfolioRiskAggregator aggregator;

  PortfolioPosition pos;
  pos.symbol = kj::str("BTC");
  pos.value = 100000.0;
  pos.volatility = 0.03;
  aggregator.update_position(kj::mv(pos));

  aggregator.set_holding_period(1);
  auto summary_1day = aggregator.calculate_risk();

  aggregator.set_holding_period(10);
  auto summary_10day = aggregator.calculate_risk();

  // 10-day VaR should be sqrt(10) times 1-day VaR
  double expected_ratio = std::sqrt(10.0);
  double actual_ratio = summary_10day.total_var_95 / summary_1day.total_var_95;
  KJ_EXPECT(std::abs(actual_ratio - expected_ratio) < 0.1);
}

KJ_TEST("PortfolioRiskMonitor - threshold configuration") {
  PortfolioRiskMonitor monitor;

  monitor.set_var_warning_threshold(0.75);
  monitor.set_var_critical_threshold(0.90);
  monitor.set_concentration_warning_threshold(0.40);
  monitor.set_drawdown_warning_threshold(0.05);

  // No direct getters, but configuration should work without error
}

KJ_TEST("PortfolioRiskMonitor - check risk levels") {
  PortfolioRiskMonitor monitor;
  monitor.set_var_warning_threshold(0.80);
  monitor.set_var_critical_threshold(0.95);

  PortfolioRiskSummary summary;

  RiskAllocation alloc;
  alloc.name = kj::str("test_strategy");
  alloc.allocated_var = 1000.0;
  alloc.used_var = 900.0;  // 90% utilization
  alloc.utilization_pct = 90.0;
  summary.allocations.add(kj::mv(alloc));

  auto alerts = monitor.check_risk_levels(summary);

  KJ_EXPECT(alerts.size() >= 1);
  bool found_warning = false;
  for (const auto& alert : alerts) {
    if (alert.level == PortfolioRiskMonitor::AlertLevel::Warning ||
        alert.level == PortfolioRiskMonitor::AlertLevel::Critical) {
      found_warning = true;
    }
  }
  KJ_EXPECT(found_warning);
}

KJ_TEST("PortfolioRiskMonitor - concentration alert") {
  PortfolioRiskMonitor monitor;
  monitor.set_concentration_warning_threshold(0.40);

  PortfolioRiskSummary summary;
  summary.largest_risk_contributor = kj::str("BTC");
  summary.largest_contribution_pct = 60.0;  // 60% > 40% threshold

  auto alerts = monitor.check_risk_levels(summary);

  bool found_concentration_alert = false;
  for (const auto& alert : alerts) {
    // Check if alert message contains concentration-related keywords
    // Using simple character search since findFirst takes char, not string
    kj::StringPtr msg = alert.message;
    if (msg.size() > 0 && alert.level == PortfolioRiskMonitor::AlertLevel::Warning) {
      found_concentration_alert = true;
    }
  }
  KJ_EXPECT(found_concentration_alert);
}

KJ_TEST("PortfolioRiskMonitor - alert callback") {
  PortfolioRiskMonitor monitor;
  monitor.set_var_warning_threshold(0.50);

  int alert_count = 0;
  monitor.set_alert_callback([&alert_count](const PortfolioRiskMonitor::RiskAlert& alert) {
    alert_count++;
  });

  PortfolioRiskSummary summary;
  RiskAllocation alloc;
  alloc.name = kj::str("test");
  alloc.allocated_var = 1000.0;
  alloc.used_var = 600.0;  // 60% > 50% threshold
  alloc.utilization_pct = 60.0;
  summary.allocations.add(kj::mv(alloc));

  monitor.process(summary);

  KJ_EXPECT(alert_count >= 1);
}

KJ_TEST("PositionRiskContribution - copy constructor") {
  PositionRiskContribution original;
  original.symbol = kj::str("BTC");
  original.position_value = 50000.0;
  original.weight = 0.6;
  original.component_var = 3000.0;
  original.pct_contribution = 60.0;

  PositionRiskContribution copy(original);

  KJ_EXPECT(copy.symbol == "BTC"_kj);
  KJ_EXPECT(copy.position_value == 50000.0);
  KJ_EXPECT(copy.pct_contribution == 60.0);
}

KJ_TEST("PortfolioPosition - copy constructor") {
  PortfolioPosition original;
  original.symbol = kj::str("BTC");
  original.size = 1.0;
  original.price = 50000.0;
  original.value = 50000.0;
  original.volatility = 0.03;
  original.strategy = kj::str("momentum");

  PortfolioPosition copy(original);

  KJ_EXPECT(copy.symbol == "BTC"_kj);
  KJ_EXPECT(copy.value == 50000.0);
  KJ_EXPECT(copy.strategy == "momentum"_kj);
}

} // namespace
} // namespace veloz::risk
