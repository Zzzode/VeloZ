#include "veloz/backtest/reporter.h"

#include "veloz/core/logger.h"

// std library includes with justifications
#include <fstream> // std::ofstream - file I/O (no KJ equivalent)

// KJ library includes
#include <kj/common.h>
#include <kj/debug.h>
#include <kj/exception.h>
#include <kj/memory.h>
#include <kj/string.h>
#include <kj/vector.h>

namespace veloz::backtest {

struct BacktestReporter::Impl {
  // kj::Own used for unique ownership of internal logger instance
  kj::Own<veloz::core::Logger> logger;

  Impl() : logger(kj::heap<veloz::core::Logger>()) {}
};

BacktestReporter::BacktestReporter() : impl_(kj::heap<Impl>()) {}

BacktestReporter::~BacktestReporter() noexcept {}

bool BacktestReporter::generate_report(const BacktestResult& result, kj::StringPtr output_path) {
  impl_->logger->info(kj::str("Generating report to: ", output_path).cStr());

  kj::String html_content = generate_html_report(result);

  // Write to file using KJ exception handling pattern
  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               std::ofstream out_file(output_path.cStr());
               if (!out_file.is_open()) {
                 impl_->logger->error(
                     kj::str("Failed to open file for writing: ", output_path).cStr());
                 return;
               }

               out_file << html_content.cStr();
               out_file.close();

               impl_->logger->info(kj::str("Report generated successfully: ", output_path).cStr());
             })) {
    impl_->logger->error(kj::str("Failed to write report: ", exception.getDescription()).cStr());
    return false;
  }

  return true;
}

kj::String BacktestReporter::generate_html_report(const BacktestResult& result) {
  // Build trade rows for the table
  kj::Vector<kj::String> trade_rows;
  for (const auto& trade : result.trades) {
    trade_rows.add(kj::str(
        R"(
                                <tr>
                                    <td>)",
        trade.timestamp,
        R"(</td>
                                    <td>)",
        trade.symbol,
        R"(</td>
                                    <td>)",
        trade.side,
        R"(</td>
                                    <td>$)",
        trade.price,
        R"(</td>
                                    <td>)",
        trade.quantity,
        R"(</td>
                                    <td>$)",
        trade.fee,
        R"(</td>
                                    <td class=)",
        (trade.pnl >= 0 ? "positive" : "negative"), ">$", trade.pnl,
        R"(</td>
                                </tr>)"));
  }

  // Build trade markers data for JavaScript
  kj::Vector<kj::String> trade_markers;
  for (const auto& trade : result.trades) {
    trade_markers.add(kj::str("{timestamp:", trade.timestamp, ",side:'", trade.side, "',price:",
                              trade.price, ",quantity:", trade.quantity, ",pnl:", trade.pnl, "}"));
  }

  // Build equity curve labels and values
  kj::Vector<kj::String> equity_labels;
  kj::Vector<kj::String> equity_values;
  for (const auto& point : result.equity_curve) {
    equity_labels.add(kj::str(point.timestamp));
    equity_values.add(kj::str(point.equity));
  }

  // Build drawdown curve labels and values
  kj::Vector<kj::String> drawdown_labels;
  kj::Vector<kj::String> drawdown_values;
  for (const auto& point : result.drawdown_curve) {
    drawdown_labels.add(kj::str(point.timestamp));
    drawdown_values.add(kj::str(point.drawdown * 100));
  }

  // Join arrays with commas
  auto join_strings = [](const kj::Vector<kj::String>& vec) -> kj::String {
    if (vec.size() == 0)
      return kj::str("");
    kj::Vector<kj::String> parts;
    for (size_t i = 0; i < vec.size(); ++i) {
      if (i > 0) {
        parts.add(kj::str(","));
      }
      parts.add(kj::str(vec[i]));
    }
    return kj::strArray(parts, "");
  };

  kj::String trade_rows_str = kj::strArray(trade_rows, "");
  kj::String trade_markers_str = join_strings(trade_markers);
  kj::String equity_labels_str = join_strings(equity_labels);
  kj::String equity_values_str = join_strings(equity_values);
  kj::String drawdown_labels_str = join_strings(drawdown_labels);
  kj::String drawdown_values_str = join_strings(drawdown_values);

  return kj::str(
      R"(
        <!DOCTYPE html>
        <html lang="en">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>VeloZ Backtest Report</title>
            <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.1/dist/chart.umd.min.js"></script>
            <script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-zoom@2.0.1/dist/chartjs-plugin-zoom.min.js"></script>
            <script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-annotation@3.0.1/dist/chartjs-plugin-annotation.min.js"></script>
            <style>
                * {
                    margin: 0;
                    padding: 0;
                    box-sizing: border-box;
                }

                body {
                    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
                    background-color: #f5f7fa;
                    color: #333;
                    line-height: 1.6;
                }

                .container {
                    max-width: 1200px;
                    margin: 0 auto;
                    padding: 20px;
                }

                .header {
                    background-color: #2c3e50;
                    color: white;
                    padding: 20px;
                    border-radius: 8px;
                    margin-bottom: 20px;
                }

                .header h1 {
                    font-size: 24px;
                    margin-bottom: 10px;
                }

                .header p {
                    font-size: 14px;
                    opacity: 0.9;
                }

                .summary {
                    display: grid;
                    grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
                    gap: 20px;
                    margin-bottom: 30px;
                }

                .stat-card {
                    background-color: white;
                    padding: 20px;
                    border-radius: 8px;
                    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
                }

                .stat-card h3 {
                    font-size: 14px;
                    margin-bottom: 10px;
                    color: #7f8c8d;
                    text-transform: uppercase;
                }

                .stat-card .value {
                    font-size: 24px;
                    font-weight: bold;
                    color: #2c3e50;
                }

                .content {
                    background-color: white;
                    padding: 20px;
                    border-radius: 8px;
                    box-shadow: 0 2px 4px rgba(0, 0, 0, 0.1);
                }

                .section {
                    margin-bottom: 30px;
                }

                .section h2 {
                    font-size: 20px;
                    margin-bottom: 15px;
                    color: #2c3e50;
                    border-bottom: 2px solid #3498db;
                    padding-bottom: 5px;
                }

                .table-container {
                    overflow-x: auto;
                    margin-top: 15px;
                }

                table {
                    width: 100%;
                    border-collapse: collapse;
                }

                th, td {
                    padding: 10px;
                    text-align: left;
                    border-bottom: 1px solid #eee;
                }

                th {
                    background-color: #f8f9fa;
                    font-weight: 600;
                    color: #2c3e50;
                }

                tr:hover {
                    background-color: #f8f9fa;
                }

                .positive {
                    color: #27ae60;
                }

                .negative {
                    color: #e74c3c;
                }

                .chart-container {
                    width: 100%;
                    height: 400px;
                    margin-top: 15px;
                    background-color: white;
                    border-radius: 4px;
                    padding: 15px;
                    position: relative;
                }

                .chart-controls {
                    display: flex;
                    gap: 10px;
                    margin-bottom: 10px;
                    flex-wrap: wrap;
                }

                .chart-controls button {
                    padding: 8px 16px;
                    border: 1px solid #3498db;
                    background-color: white;
                    color: #3498db;
                    border-radius: 4px;
                    cursor: pointer;
                    font-size: 12px;
                    transition: all 0.2s ease;
                }

                .chart-controls button:hover {
                    background-color: #3498db;
                    color: white;
                }

                .chart-controls button.active {
                    background-color: #3498db;
                    color: white;
                }

                .legend-custom {
                    display: flex;
                    gap: 20px;
                    margin-top: 10px;
                    font-size: 12px;
                    flex-wrap: wrap;
                }

                .legend-item {
                    display: flex;
                    align-items: center;
                    gap: 5px;
                }

                .legend-marker {
                    width: 12px;
                    height: 12px;
                    border-radius: 50%;
                }

                .legend-marker.buy {
                    background-color: #27ae60;
                }

                .legend-marker.sell {
                    background-color: #e74c3c;
                }

                .legend-marker.equity {
                    background-color: #3498db;
                }

                .legend-marker.drawdown {
                    background-color: #e74c3c;
                }

                @media (max-width: 768px) {
                    .summary {
                        grid-template-columns: repeat(2, 1fr);
                    }

                    .chart-container {
                        height: 300px;
                    }

                    .header h1 {
                        font-size: 20px;
                    }
                }

                @media (max-width: 480px) {
                    .summary {
                        grid-template-columns: 1fr;
                    }

                    .stat-card .value {
                        font-size: 20px;
                    }
                }
            </style>
        </head>
        <body>
            <div class="container">
                <div class="header">
                    <h1>VeloZ Backtest Report</h1>
                    <p>Strategy Name: )",
      result.strategy_name, R"( | Trading Pair: )", result.symbol,
      R"(</p>
                    <p>Backtest Period: )",
      result.start_time, R"( - )", result.end_time,
      R"(</p>
                </div>

                <div class="summary">
                    <div class="stat-card">
                        <h3>Initial Balance</h3>
                        <div class="value">\$)",
      result.initial_balance,
      R"(</div>
                    </div>

                    <div class="stat-card">
                        <h3>Final Balance</h3>
                        <div class="value">$)",
      result.final_balance,
      R"(</div>
                    </div>

                    <div class="stat-card">
                        <h3>Total Return</h3>
                        <div class="value )",
      (result.total_return >= 0 ? "positive" : "negative"), ">", (result.total_return * 100),
      R"(%</div>
                    </div>

                    <div class="stat-card">
                        <h3>Max Drawdown</h3>
                        <div class="value )",
      (result.max_drawdown >= 0 ? "negative" : "positive"), ">", (result.max_drawdown * 100),
      R"(%</div>
                    </div>

                    <div class="stat-card">
                        <h3>Sharpe Ratio</h3>
                        <div class="value">)",
      result.sharpe_ratio,
      R"(</div>
                    </div>

                    <div class="stat-card">
                        <h3>Win Rate</h3>
                        <div class="value )",
      (result.win_rate >= 0.5 ? "positive" : "negative"), ">", (result.win_rate * 100),
      R"(%</div>
                    </div>
                </div>

                <div class="content">
                    <div class="section">
                        <h2>Detailed Results</h2>

                        <div class="table-container">
                            <table>
                                <tr>
                                    <th>Metric</th>
                                    <th>Value</th>
                                </tr>
                                <tr>
                                    <td>Total Trades</td>
                                    <td>)",
      result.trade_count,
      R"()</td>
                                </tr>
                                <tr>
                                    <td>Winning Trades</td>
                                    <td>)",
      result.win_count,
      R"()</td>
                                </tr>
                                <tr>
                                    <td>Losing Trades</td>
                                    <td>)",
      result.lose_count,
      R"()</td>
                                </tr>
                                <tr>
                                    <td>Profit Factor</td>
                                    <td>)",
      result.profit_factor,
      R"(</td>
                                </tr>
                                <tr>
                                    <td>Average Win</td>
                                    <td>$)",
      result.avg_win,
      R"(</td>
                                </tr>
                                <tr>
                                    <td>Average Loss</td>
                                    <td class=")",
      (result.avg_lose < 0 ? "negative" : "positive"), ">$", result.avg_lose,
      R"(</td>
                                </tr>
                            </table>
                        </div>
                    </div>

                    <div class="section">
                        <h2>Equity Curve</h2>
                        <div class="chart-controls">
                            <button id="resetEquityZoom">Reset Zoom</button>
                            <button id="toggleEquityMarkers" class="active">Toggle Trade Markers</button>
                        </div>
                        <div class="chart-container">
                            <canvas id="equityChart"></canvas>
                        </div>
                        <div class="legend-custom">
                            <div class="legend-item"><div class="legend-marker equity"></div><span>Equity</span></div>
                            <div class="legend-item"><div class="legend-marker buy"></div><span>Buy Trade</span></div>
                            <div class="legend-item"><div class="legend-marker sell"></div><span>Sell Trade</span></div>
                        </div>
                    </div>

                    <div class="section">
                        <h2>Drawdown Curve</h2>
                        <div class="chart-controls">
                            <button id="resetDrawdownZoom">Reset Zoom</button>
                            <button id="toggleDrawdownMarkers" class="active">Toggle Trade Markers</button>
                        </div>
                        <div class="chart-container">
                            <canvas id="drawdownChart"></canvas>
                        </div>
                        <div class="legend-custom">
                            <div class="legend-item"><div class="legend-marker drawdown"></div><span>Drawdown</span></div>
                            <div class="legend-item"><div class="legend-marker buy"></div><span>Buy Trade</span></div>
                            <div class="legend-item"><div class="legend-marker sell"></div><span>Sell Trade</span></div>
                        </div>
                    </div>

                    <div class="section">
                        <h2>Trade History</h2>
                        <div class="table-container">
                            <table>
                                <tr>
                                    <th>Time</th>
                                    <th>Symbol</th>
                                    <th>Side</th>
                                    <th>Price</th>
                                    <th>Quantity</th>
                                    <th>Fee</th>
                                    <th>P&L</th>
                                </tr>)",
      trade_rows_str,
      R"(
                            </table>
                        </div>
                    </div>
                </div>
            </div>

            <script>
                // Helper function to format timestamp to readable date
                function formatTimestamp(ts) {
                    const date = new Date(ts);
                    return date.toLocaleDateString('en-US', { month: 'short', day: 'numeric', hour: '2-digit', minute: '2-digit' });
                }

                // Trade markers data
                const tradeMarkers = [)",
      trade_markers_str,
      R"(];

                // Generate equity curve data
                const equityLabels = [)",
      equity_labels_str,
      R"(];
                const equityValues = [)",
      equity_values_str,
      R"(];

                // Generate drawdown curve data
                const drawdownLabels = [)",
      drawdown_labels_str,
      R"(];
                const drawdownValues = [)",
      drawdown_values_str,
      R"(];

                // Find nearest equity value for a given timestamp
                function findNearestEquity(timestamp) {
                    let nearestIdx = 0;
                    let minDiff = Math.abs(equityLabels[0] - timestamp);
                    for (let i = 1; i < equityLabels.length; i++) {
                        const diff = Math.abs(equityLabels[i] - timestamp);
                        if (diff < minDiff) {
                            minDiff = diff;
                            nearestIdx = i;
                        }
                    }
                    return { index: nearestIdx, value: equityValues[nearestIdx] };
                }

                // Find nearest drawdown value for a given timestamp
                function findNearestDrawdown(timestamp) {
                    let nearestIdx = 0;
                    let minDiff = Math.abs(drawdownLabels[0] - timestamp);
                    for (let i = 1; i < drawdownLabels.length; i++) {
                        const diff = Math.abs(drawdownLabels[i] - timestamp);
                        if (diff < minDiff) {
                            minDiff = diff;
                            nearestIdx = i;
                        }
                    }
                    return { index: nearestIdx, value: drawdownValues[nearestIdx] };
                }

                // Create buy/sell marker datasets for equity chart
                const buyMarkersEquity = tradeMarkers.filter(t => t.side === 'buy').map(t => {
                    const nearest = findNearestEquity(t.timestamp);
                    return { x: nearest.index, y: nearest.value, trade: t };
                });
                const sellMarkersEquity = tradeMarkers.filter(t => t.side === 'sell').map(t => {
                    const nearest = findNearestEquity(t.timestamp);
                    return { x: nearest.index, y: nearest.value, trade: t };
                });

                // Create buy/sell marker datasets for drawdown chart
                const buyMarkersDrawdown = tradeMarkers.filter(t => t.side === 'buy').map(t => {
                    const nearest = findNearestDrawdown(t.timestamp);
                    return { x: nearest.index, y: nearest.value, trade: t };
                });
                const sellMarkersDrawdown = tradeMarkers.filter(t => t.side === 'sell').map(t => {
                    const nearest = findNearestDrawdown(t.timestamp);
                    return { x: nearest.index, y: nearest.value, trade: t };
                });

                // Equity chart data with trade markers
                const equityData = {
                    labels: equityLabels.map(String),
                    datasets: [
                        {
                            label: 'Equity ($)',
                            data: equityValues,
                            borderColor: '#3498db',
                            backgroundColor: 'rgba(52, 152, 219, 0.1)',
                            borderWidth: 2,
                            fill: true,
                            tension: 0.1,
                            pointRadius: 0,
                            pointHoverRadius: 5,
                            order: 2
                        },
                        {
                            label: 'Buy',
                            data: buyMarkersEquity,
                            type: 'scatter',
                            backgroundColor: '#27ae60',
                            borderColor: '#1e8449',
                            borderWidth: 2,
                            pointRadius: 8,
                            pointHoverRadius: 10,
                            pointStyle: 'triangle',
                            order: 1
                        },
                        {
                            label: 'Sell',
                            data: sellMarkersEquity,
                            type: 'scatter',
                            backgroundColor: '#e74c3c',
                            borderColor: '#c0392b',
                            borderWidth: 2,
                            pointRadius: 8,
                            pointHoverRadius: 10,
                            pointStyle: 'rectRot',
                            order: 1
                        }
                    ]
                };

                // Drawdown chart data with trade markers
                const drawdownData = {
                    labels: drawdownLabels.map(String),
                    datasets: [
                        {
                            label: 'Drawdown (%)',
                            data: drawdownValues,
                            borderColor: '#e74c3c',
                            backgroundColor: 'rgba(231, 76, 60, 0.2)',
                            borderWidth: 2,
                            fill: true,
                            tension: 0.1,
                            pointRadius: 0,
                            pointHoverRadius: 5,
                            order: 2
                        },
                        {
                            label: 'Buy',
                            data: buyMarkersDrawdown,
                            type: 'scatter',
                            backgroundColor: '#27ae60',
                            borderColor: '#1e8449',
                            borderWidth: 2,
                            pointRadius: 8,
                            pointHoverRadius: 10,
                            pointStyle: 'triangle',
                            order: 1
                        },
                        {
                            label: 'Sell',
                            data: sellMarkersDrawdown,
                            type: 'scatter',
                            backgroundColor: '#e74c3c',
                            borderColor: '#c0392b',
                            borderWidth: 2,
                            pointRadius: 8,
                            pointHoverRadius: 10,
                            pointStyle: 'rectRot',
                            order: 1
                        }
                    ]
                };

                // Chart instances storage
                let equityChart = null;
                let drawdownChart = null;
                let showEquityMarkers = true;
                let showDrawdownMarkers = true;

                // Common chart options with zoom plugin
                const commonOptions = {
                    responsive: true,
                    maintainAspectRatio: false,
                    interaction: {
                        mode: 'index',
                        intersect: false
                    },
                    plugins: {
                        legend: {
                            display: false
                        },
                        tooltip: {
                            callbacks: {
                                title: function(context) {
                                    const label = context[0].label || context[0].raw?.x;
                                    return formatTimestamp(parseInt(label));
                                },
                                label: function(context) {
                                    if (context.raw && context.raw.trade) {
                                        const t = context.raw.trade;
                                        return [
                                            context.dataset.label + ' Trade',
                                            'Price: $' + t.price.toFixed(2),
                                            'Qty: ' + t.quantity.toFixed(4),
                                            'P&L: $' + t.pnl.toFixed(2)
                                        ];
                                    }
                                    return context.dataset.label + ': ' + context.formattedValue;
                                }
                            }
                        },
                        zoom: {
                            pan: {
                                enabled: true,
                                mode: 'x'
                            },
                            zoom: {
                                wheel: {
                                    enabled: true
                                },
                                pinch: {
                                    enabled: true
                                },
                                drag: {
                                    enabled: true,
                                    backgroundColor: 'rgba(52, 152, 219, 0.2)',
                                    borderColor: '#3498db',
                                    borderWidth: 1
                                },
                                mode: 'x'
                            }
                        }
                    },
                    scales: {
                        x: {
                            grid: {
                                display: false
                            },
                            ticks: {
                                maxTicksLimit: 10,
                                callback: function(value, index) {
                                    const label = this.getLabelForValue(value);
                                    return formatTimestamp(parseInt(label));
                                }
                            }
                        },
                        y: {
                            grid: {
                                color: 'rgba(0, 0, 0, 0.05)'
                            }
                        }
                    }
                };

                // Reset zoom function
                function resetZoom(chartType) {
                    if (chartType === 'equity' && equityChart) {
                        equityChart.resetZoom();
                    } else if (chartType === 'drawdown' && drawdownChart) {
                        drawdownChart.resetZoom();
                    }
                }

                // Toggle trade markers function
                function toggleTradeMarkers(chartType) {
                    if (chartType === 'equity' && equityChart) {
                        showEquityMarkers = !showEquityMarkers;
                        equityChart.data.datasets[1].hidden = !showEquityMarkers;
                        equityChart.data.datasets[2].hidden = !showEquityMarkers;
                        equityChart.update();
                        document.getElementById('toggleEquityMarkers').classList.toggle('active', showEquityMarkers);
                    } else if (chartType === 'drawdown' && drawdownChart) {
                        showDrawdownMarkers = !showDrawdownMarkers;
                        drawdownChart.data.datasets[1].hidden = !showDrawdownMarkers;
                        drawdownChart.data.datasets[2].hidden = !showDrawdownMarkers;
                        drawdownChart.update();
                        document.getElementById('toggleDrawdownMarkers').classList.toggle('active', showDrawdownMarkers);
                    }
                }

                // Initialize Equity Chart
                const equityCtx = document.getElementById('equityChart').getContext('2d');
                equityChart = new Chart(equityCtx, {
                    type: 'line',
                    data: equityData,
                    options: {
                        ...commonOptions,
                        plugins: {
                            ...commonOptions.plugins,
                            title: {
                                display: true,
                                text: 'Equity Curve Over Time (scroll to zoom, drag to pan)'
                            }
                        },
                        scales: {
                            ...commonOptions.scales,
                            y: {
                                ...commonOptions.scales.y,
                                title: {
                                    display: true,
                                    text: 'Equity ($)'
                                }
                            }
                        }
                    }
                });

                // Initialize Drawdown Chart
                const drawdownCtx = document.getElementById('drawdownChart').getContext('2d');
                drawdownChart = new Chart(drawdownCtx, {
                    type: 'line',
                    data: drawdownData,
                    options: {
                        ...commonOptions,
                        plugins: {
                            ...commonOptions.plugins,
                            title: {
                                display: true,
                                text: 'Drawdown Over Time (scroll to zoom, drag to pan)'
                            }
                        },
                        scales: {
                            ...commonOptions.scales,
                            y: {
                                ...commonOptions.scales.y,
                                title: {
                                    display: true,
                                    text: 'Drawdown (%)'
                                },
                                ticks: {
                                    callback: function(value) {
                                        return value.toFixed(2) + '%';
                                    }
                                }
                            }
                        }
                    }
                });

                // Add event listeners for buttons
                document.getElementById('resetEquityZoom').addEventListener('click', function() {
                    resetZoom('equity');
                });
                document.getElementById('toggleEquityMarkers').addEventListener('click', function() {
                    toggleTradeMarkers('equity');
                });
                document.getElementById('resetDrawdownZoom').addEventListener('click', function() {
                    resetZoom('drawdown');
                });
                document.getElementById('toggleDrawdownMarkers').addEventListener('click', function() {
                    toggleTradeMarkers('drawdown');
                });
            </script>
        </body>
        </html>
    )");
}

kj::String BacktestReporter::generate_json_report(const BacktestResult& result) {
  // Build trade records array
  kj::Vector<kj::String> trade_records;
  for (const auto& trade : result.trades) {
    trade_records.add(kj::str(R"(
            {
                "timestamp": )",
                              trade.timestamp, R"(,
                "symbol": ")",
                              trade.symbol, R"(",
                "side": ")",
                              trade.side, R"(",
                "price": )",
                              trade.price, R"(,
                "quantity": )",
                              trade.quantity, R"(,
                "fee": )",
                              trade.fee, R"(,
                "pnl": )",
                              trade.pnl, R"(,
                "strategy_id": ")",
                              trade.strategy_id, R"("
            })"));
  }

  kj::String trades_str = kj::strArray(trade_records, ",");

  return kj::str(R"({
        "strategy_name": ")",
                 result.strategy_name, R"(",
        "symbol": ")",
                 result.symbol, R"(",
        "start_time": )",
                 result.start_time, R"(,
        "end_time": )",
                 result.end_time, R"(,
        "initial_balance": )",
                 result.initial_balance, R"(,
        "final_balance": )",
                 result.final_balance, R"(,
        "total_return": )",
                 result.total_return, R"(,
        "max_drawdown": )",
                 result.max_drawdown, R"(,
        "sharpe_ratio": )",
                 result.sharpe_ratio, R"(,
        "win_rate": )",
                 result.win_rate, R"(,
        "profit_factor": )",
                 result.profit_factor, R"(,
        "trade_count": )",
                 result.trade_count, R"(,
        "win_count": )",
                 result.win_count, R"(,
        "lose_count": )",
                 result.lose_count, R"(,
        "avg_win": )",
                 result.avg_win, R"(,
        "avg_lose": )",
                 result.avg_lose, R"(,
        "trades": [)",
                 trades_str, R"(
        ]
    })");
}

} // namespace veloz::backtest
