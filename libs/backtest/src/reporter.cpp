#include "veloz/backtest/reporter.h"

#include "veloz/core/logger.h"

// std library includes with justifications
#include <algorithm> // std::sort, std::min, std::max - standard algorithms
#include <cmath>     // std::sqrt, std::pow - math functions (no KJ equivalent)
#include <ctime>     // std::localtime, std::gmtime - time conversion (no KJ equivalent)
#include <fstream>   // std::ofstream - file I/O (no KJ equivalent)
#include <iomanip>   // std::setprecision - output formatting (no KJ equivalent)
#include <numeric>   // std::accumulate - numeric algorithms (no KJ equivalent)
#include <sstream>   // std::ostringstream - string stream (no KJ equivalent)
#include <vector> // std::vector - for std::sort compatibility (kj::Vector lacks random access iterators)

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
  ReportConfig config;

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

void BacktestReporter::set_config(const ReportConfig& config) {
  impl_->config.include_equity_curve = config.include_equity_curve;
  impl_->config.include_drawdown_curve = config.include_drawdown_curve;
  impl_->config.include_trade_list = config.include_trade_list;
  impl_->config.include_monthly_returns = config.include_monthly_returns;
  impl_->config.include_trade_analysis = config.include_trade_analysis;
  impl_->config.include_risk_metrics = config.include_risk_metrics;
  impl_->config.title = kj::str(config.title);
  impl_->config.description = kj::str(config.description);
  impl_->config.author = kj::str(config.author);
}

const ReportConfig& BacktestReporter::get_config() const {
  return impl_->config;
}

bool BacktestReporter::generate_report_format(const BacktestResult& result,
                                              kj::StringPtr output_path, ReportFormat format) {
  impl_->logger->info(
      kj::str("Generating report in format ", static_cast<int>(format), " to: ", output_path)
          .cStr());

  kj::String content;
  switch (format) {
  case ReportFormat::HTML:
    content = generate_html_report(result);
    break;
  case ReportFormat::JSON:
    content = generate_json_report(result);
    break;
  case ReportFormat::CSV:
    content = generate_csv_trades(result);
    break;
  case ReportFormat::Markdown:
    content = generate_markdown_report(result);
    break;
  }

  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               std::ofstream out_file(output_path.cStr());
               if (!out_file.is_open()) {
                 impl_->logger->error(
                     kj::str("Failed to open file for writing: ", output_path).cStr());
                 return;
               }
               out_file << content.cStr();
               out_file.close();
               impl_->logger->info(kj::str("Report generated successfully: ", output_path).cStr());
             })) {
    impl_->logger->error(kj::str("Failed to write report: ", exception.getDescription()).cStr());
    return false;
  }

  return true;
}

kj::String BacktestReporter::generate_csv_trades(const BacktestResult& result) {
  std::ostringstream csv;
  csv << "timestamp,symbol,side,price,quantity,fee,pnl,strategy_id\n";

  for (const auto& trade : result.trades) {
    csv << trade.timestamp << "," << trade.symbol.cStr() << "," << trade.side.cStr() << ","
        << std::fixed << std::setprecision(8) << trade.price << "," << trade.quantity << ","
        << trade.fee << "," << trade.pnl << "," << trade.strategy_id.cStr() << "\n";
  }

  return kj::str(csv.str().c_str());
}

kj::String BacktestReporter::generate_markdown_report(const BacktestResult& result) {
  auto monthly_returns = calculate_monthly_returns(result);
  auto trade_analysis = analyze_trades(result);
  auto extended_metrics = calculate_extended_metrics(result);

  std::ostringstream md;

  // Header
  md << "# "
     << (impl_->config.title.size() > 0 ? impl_->config.title.cStr() : "VeloZ Backtest Report")
     << "\n\n";

  if (impl_->config.description.size() > 0) {
    md << impl_->config.description.cStr() << "\n\n";
  }

  // Summary
  md << "## Summary\n\n";
  md << "| Metric | Value |\n";
  md << "|--------|-------|\n";
  md << "| Strategy | " << result.strategy_name.cStr() << " |\n";
  md << "| Symbol | " << result.symbol.cStr() << " |\n";
  md << "| Initial Balance | $" << std::fixed << std::setprecision(2) << result.initial_balance
     << " |\n";
  md << "| Final Balance | $" << result.final_balance << " |\n";
  md << "| Total Return | " << std::setprecision(2) << (result.total_return * 100) << "% |\n";
  md << "| Max Drawdown | " << (result.max_drawdown * 100) << "% |\n";
  md << "| Sharpe Ratio | " << std::setprecision(3) << result.sharpe_ratio << " |\n";
  md << "| Win Rate | " << std::setprecision(2) << (result.win_rate * 100) << "% |\n";
  md << "| Profit Factor | " << std::setprecision(3) << result.profit_factor << " |\n";
  md << "| Total Trades | " << result.trade_count << " |\n\n";

  // Extended Risk Metrics
  if (impl_->config.include_risk_metrics) {
    md << "## Risk Metrics\n\n";
    md << "| Metric | Value |\n";
    md << "|--------|-------|\n";
    md << "| Sortino Ratio | " << std::setprecision(3) << extended_metrics.sortino_ratio << " |\n";
    md << "| Calmar Ratio | " << extended_metrics.calmar_ratio << " |\n";
    md << "| Omega Ratio | " << extended_metrics.omega_ratio << " |\n";
    md << "| VaR (95%) | " << std::setprecision(2) << (extended_metrics.value_at_risk_95 * 100)
       << "% |\n";
    md << "| Expected Shortfall (95%) | " << (extended_metrics.expected_shortfall_95 * 100)
       << "% |\n";
    md << "| Recovery Factor | " << std::setprecision(3) << extended_metrics.recovery_factor
       << " |\n";
    md << "| Ulcer Index | " << extended_metrics.ulcer_index << " |\n\n";
  }

  // Trade Analysis
  if (impl_->config.include_trade_analysis) {
    md << "## Trade Analysis\n\n";
    md << "| Metric | Value |\n";
    md << "|--------|-------|\n";
    md << "| Best Trade P&L | $" << std::setprecision(2) << trade_analysis.best_trade_pnl << " |\n";
    md << "| Worst Trade P&L | $" << trade_analysis.worst_trade_pnl << " |\n";
    md << "| Max Consecutive Wins | " << trade_analysis.max_consecutive_wins << " |\n";
    md << "| Max Consecutive Losses | " << trade_analysis.max_consecutive_losses << " |\n";
    md << "| Avg Trade Duration | " << std::setprecision(0)
       << (trade_analysis.avg_trade_duration_ms / 1000.0) << "s |\n\n";
  }

  // Monthly Returns
  if (impl_->config.include_monthly_returns && monthly_returns.size() > 0) {
    md << "## Monthly Returns\n\n";
    md << "| Year | Month | Return | Trades | Max DD |\n";
    md << "|------|-------|--------|--------|--------|\n";
    for (const auto& mr : monthly_returns) {
      md << "| " << mr.year << " | " << mr.month << " | " << std::setprecision(2)
         << (mr.return_pct * 100) << "% | " << mr.trade_count << " | " << (mr.max_drawdown * 100)
         << "% |\n";
    }
    md << "\n";
  }

  // Trade List
  if (impl_->config.include_trade_list && result.trades.size() > 0) {
    md << "## Trade History\n\n";
    md << "| Time | Symbol | Side | Price | Qty | Fee | P&L |\n";
    md << "|------|--------|------|-------|-----|-----|-----|\n";

    // Limit to first 100 trades for readability
    size_t max_trades = std::min(result.trades.size(), static_cast<size_t>(100));
    for (size_t i = 0; i < max_trades; ++i) {
      const auto& trade = result.trades[i];
      md << "| " << trade.timestamp << " | " << trade.symbol.cStr() << " | " << trade.side.cStr()
         << " | $" << std::setprecision(2) << trade.price << " | " << std::setprecision(4)
         << trade.quantity << " | $" << std::setprecision(2) << trade.fee << " | $" << trade.pnl
         << " |\n";
    }
    if (result.trades.size() > 100) {
      md << "\n*... and " << (result.trades.size() - 100) << " more trades*\n";
    }
    md << "\n";
  }

  // Footer
  if (impl_->config.author.size() > 0) {
    md << "---\n*Generated by " << impl_->config.author.cStr() << "*\n";
  }

  return kj::str(md.str().c_str());
}

kj::Vector<MonthlyReturn>
BacktestReporter::calculate_monthly_returns(const BacktestResult& result) {
  kj::Vector<MonthlyReturn> monthly_returns;

  if (result.equity_curve.size() == 0) {
    return monthly_returns;
  }

  // Group equity points by month
  struct MonthData {
    int year;
    int month;
    double start_equity;
    double end_equity;
    double min_equity;
    int trade_count;
  };

  kj::Vector<MonthData> month_data;
  int current_year = -1;
  int current_month = -1;

  for (const auto& point : result.equity_curve) {
    // Convert timestamp to year/month
    std::time_t time_sec = static_cast<std::time_t>(point.timestamp / 1000);
    std::tm* tm_info = std::gmtime(&time_sec);
    if (!tm_info)
      continue;

    int year = tm_info->tm_year + 1900;
    int month = tm_info->tm_mon + 1;

    if (year != current_year || month != current_month) {
      // New month
      if (month_data.size() > 0) {
        // Update end equity of previous month
        month_data[month_data.size() - 1].end_equity = point.equity;
      }

      MonthData md;
      md.year = year;
      md.month = month;
      md.start_equity = point.equity;
      md.end_equity = point.equity;
      md.min_equity = point.equity;
      md.trade_count = 0;
      month_data.add(md);

      current_year = year;
      current_month = month;
    } else {
      // Same month - update end equity and min
      auto& md = month_data[month_data.size() - 1];
      md.end_equity = point.equity;
      if (point.equity < md.min_equity) {
        md.min_equity = point.equity;
      }
    }
  }

  // Count trades per month
  for (const auto& trade : result.trades) {
    std::time_t time_sec = static_cast<std::time_t>(trade.timestamp / 1000);
    std::tm* tm_info = std::gmtime(&time_sec);
    if (!tm_info)
      continue;

    int year = tm_info->tm_year + 1900;
    int month = tm_info->tm_mon + 1;

    for (auto& md : month_data) {
      if (md.year == year && md.month == month) {
        md.trade_count++;
        break;
      }
    }
  }

  // Calculate returns
  for (const auto& md : month_data) {
    MonthlyReturn mr;
    mr.year = md.year;
    mr.month = md.month;
    mr.return_pct = md.start_equity > 0 ? (md.end_equity - md.start_equity) / md.start_equity : 0.0;
    mr.trade_count = md.trade_count;
    mr.max_drawdown =
        md.start_equity > 0 ? (md.start_equity - md.min_equity) / md.start_equity : 0.0;
    monthly_returns.add(mr);
  }

  return monthly_returns;
}

TradeAnalysis BacktestReporter::analyze_trades(const BacktestResult& result) {
  TradeAnalysis analysis;

  if (result.trades.size() == 0) {
    return analysis;
  }

  // Initialize with first trade
  analysis.best_trade_pnl = result.trades[0].pnl;
  analysis.worst_trade_pnl = result.trades[0].pnl;
  analysis.best_trade_timestamp = result.trades[0].timestamp;
  analysis.worst_trade_timestamp = result.trades[0].timestamp;

  int consecutive_wins = 0;
  int consecutive_losses = 0;
  bool last_was_win = false;

  double total_duration = 0.0;
  double total_winning_duration = 0.0;
  double total_losing_duration = 0.0;
  int winning_trades = 0;
  int losing_trades = 0;

  for (size_t i = 0; i < result.trades.size(); ++i) {
    const auto& trade = result.trades[i];

    // Best/worst trades
    if (trade.pnl > analysis.best_trade_pnl) {
      analysis.best_trade_pnl = trade.pnl;
      analysis.best_trade_timestamp = trade.timestamp;
    }
    if (trade.pnl < analysis.worst_trade_pnl) {
      analysis.worst_trade_pnl = trade.pnl;
      analysis.worst_trade_timestamp = trade.timestamp;
    }

    // Consecutive wins/losses
    bool is_win = trade.pnl > 0;
    if (i == 0) {
      consecutive_wins = is_win ? 1 : 0;
      consecutive_losses = is_win ? 0 : 1;
      last_was_win = is_win;
    } else {
      if (is_win) {
        if (last_was_win) {
          consecutive_wins++;
        } else {
          consecutive_wins = 1;
          consecutive_losses = 0;
        }
        winning_trades++;
      } else {
        if (!last_was_win) {
          consecutive_losses++;
        } else {
          consecutive_losses = 1;
          consecutive_wins = 0;
        }
        losing_trades++;
      }
      last_was_win = is_win;
    }

    if (consecutive_wins > analysis.max_consecutive_wins) {
      analysis.max_consecutive_wins = consecutive_wins;
    }
    if (consecutive_losses > analysis.max_consecutive_losses) {
      analysis.max_consecutive_losses = consecutive_losses;
    }

    // Trade duration (estimate from consecutive trades)
    if (i > 0) {
      double duration = static_cast<double>(trade.timestamp - result.trades[i - 1].timestamp);
      total_duration += duration;

      if (analysis.max_trade_duration_ms == 0.0 || duration > analysis.max_trade_duration_ms) {
        analysis.max_trade_duration_ms = duration;
      }
      if (analysis.min_trade_duration_ms == 0.0 || duration < analysis.min_trade_duration_ms) {
        analysis.min_trade_duration_ms = duration;
      }

      if (is_win) {
        total_winning_duration += duration;
      } else {
        total_losing_duration += duration;
      }
    }
  }

  // Calculate averages
  if (result.trades.size() > 1) {
    analysis.avg_trade_duration_ms = total_duration / static_cast<double>(result.trades.size() - 1);
  }
  if (winning_trades > 0) {
    analysis.avg_winning_duration_ms = total_winning_duration / static_cast<double>(winning_trades);
  }
  if (losing_trades > 0) {
    analysis.avg_losing_duration_ms = total_losing_duration / static_cast<double>(losing_trades);
  }

  // Current streak
  analysis.current_streak = last_was_win ? consecutive_wins : consecutive_losses;
  analysis.current_streak_winning = last_was_win;

  return analysis;
}

ExtendedRiskMetrics BacktestReporter::calculate_extended_metrics(const BacktestResult& result) {
  ExtendedRiskMetrics metrics;

  if (result.trades.size() < 2) {
    return metrics;
  }

  // Collect returns
  std::vector<double> returns;
  std::vector<double> negative_returns;

  for (const auto& trade : result.trades) {
    double ret = trade.pnl / result.initial_balance;
    returns.push_back(ret);
    if (ret < 0) {
      negative_returns.push_back(ret);
    }
  }

  // Calculate mean return
  double mean_return = 0.0;
  for (double r : returns) {
    mean_return += r;
  }
  mean_return /= static_cast<double>(returns.size());

  // Calculate standard deviation
  double variance = 0.0;
  for (double r : returns) {
    variance += (r - mean_return) * (r - mean_return);
  }
  variance /= static_cast<double>(returns.size());
  double std_dev = std::sqrt(variance);

  // Calculate downside deviation (for Sortino)
  double downside_variance = 0.0;
  for (double r : returns) {
    if (r < 0) {
      downside_variance += r * r;
    }
  }
  downside_variance /= static_cast<double>(returns.size());
  double downside_dev = std::sqrt(downside_variance);

  // Sortino Ratio
  if (downside_dev > 0) {
    metrics.sortino_ratio = mean_return / downside_dev * std::sqrt(252.0); // Annualized
  }

  // Calmar Ratio
  if (result.max_drawdown > 0) {
    metrics.calmar_ratio = result.total_return / result.max_drawdown;
  }

  // Omega Ratio (threshold = 0)
  double gains = 0.0;
  double losses = 0.0;
  for (double r : returns) {
    if (r > 0) {
      gains += r;
    } else {
      losses += std::abs(r);
    }
  }
  if (losses > 0) {
    metrics.omega_ratio = gains / losses;
  }

  // Value at Risk (95%)
  std::vector<double> sorted_returns = returns;
  std::sort(sorted_returns.begin(), sorted_returns.end());
  size_t var_index = static_cast<size_t>(sorted_returns.size() * 0.05);
  if (var_index < sorted_returns.size()) {
    metrics.value_at_risk_95 = sorted_returns[var_index];
  }

  // Expected Shortfall (CVaR 95%)
  if (var_index > 0) {
    double es_sum = 0.0;
    for (size_t i = 0; i < var_index; ++i) {
      es_sum += sorted_returns[i];
    }
    metrics.expected_shortfall_95 = es_sum / static_cast<double>(var_index);
  }

  // Skewness
  if (std_dev > 0) {
    double skew_sum = 0.0;
    for (double r : returns) {
      skew_sum += std::pow((r - mean_return) / std_dev, 3);
    }
    metrics.skewness = skew_sum / static_cast<double>(returns.size());
  }

  // Kurtosis
  if (std_dev > 0) {
    double kurt_sum = 0.0;
    for (double r : returns) {
      kurt_sum += std::pow((r - mean_return) / std_dev, 4);
    }
    metrics.kurtosis = kurt_sum / static_cast<double>(returns.size()) - 3.0; // Excess kurtosis
  }

  // Recovery Factor
  if (result.max_drawdown > 0) {
    metrics.recovery_factor = result.total_return / result.max_drawdown;
  }

  // Ulcer Index
  if (result.drawdown_curve.size() > 0) {
    double sum_sq_dd = 0.0;
    for (const auto& dd : result.drawdown_curve) {
      sum_sq_dd += dd.drawdown * dd.drawdown;
    }
    metrics.ulcer_index = std::sqrt(sum_sq_dd / static_cast<double>(result.drawdown_curve.size()));
  }

  // Tail Ratio
  size_t tail_size = std::max(static_cast<size_t>(1), sorted_returns.size() / 20);
  double upper_tail = 0.0;
  double lower_tail = 0.0;
  for (size_t i = 0; i < tail_size; ++i) {
    lower_tail += std::abs(sorted_returns[i]);
    upper_tail += sorted_returns[sorted_returns.size() - 1 - i];
  }
  if (lower_tail > 0) {
    metrics.tail_ratio = upper_tail / lower_tail;
  }

  return metrics;
}

bool BacktestReporter::export_equity_curve_csv(const BacktestResult& result,
                                               kj::StringPtr output_path) {
  impl_->logger->info(kj::str("Exporting equity curve to: ", output_path).cStr());

  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               std::ofstream out_file(output_path.cStr());
               if (!out_file.is_open()) {
                 impl_->logger->error(
                     kj::str("Failed to open file for writing: ", output_path).cStr());
                 return;
               }

               out_file << "timestamp,equity,cumulative_return\n";
               for (const auto& point : result.equity_curve) {
                 out_file << point.timestamp << "," << std::fixed << std::setprecision(8)
                          << point.equity << "," << point.cumulative_return << "\n";
               }
               out_file.close();
               impl_->logger->info(kj::str("Equity curve exported: ", output_path).cStr());
             })) {
    impl_->logger->error(
        kj::str("Failed to export equity curve: ", exception.getDescription()).cStr());
    return false;
  }

  return true;
}

bool BacktestReporter::export_drawdown_curve_csv(const BacktestResult& result,
                                                 kj::StringPtr output_path) {
  impl_->logger->info(kj::str("Exporting drawdown curve to: ", output_path).cStr());

  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               std::ofstream out_file(output_path.cStr());
               if (!out_file.is_open()) {
                 impl_->logger->error(
                     kj::str("Failed to open file for writing: ", output_path).cStr());
                 return;
               }

               out_file << "timestamp,drawdown\n";
               for (const auto& point : result.drawdown_curve) {
                 out_file << point.timestamp << "," << std::fixed << std::setprecision(8)
                          << point.drawdown << "\n";
               }
               out_file.close();
               impl_->logger->info(kj::str("Drawdown curve exported: ", output_path).cStr());
             })) {
    impl_->logger->error(
        kj::str("Failed to export drawdown curve: ", exception.getDescription()).cStr());
    return false;
  }

  return true;
}

bool BacktestReporter::generate_comparison_report(kj::ArrayPtr<const BacktestResult> results,
                                                  kj::StringPtr output_path) {
  impl_->logger->info(
      kj::str("Generating comparison report for ", results.size(), " results to: ", output_path)
          .cStr());

  if (results.size() == 0) {
    impl_->logger->error("No results to compare");
    return false;
  }

  std::ostringstream html;

  // HTML header
  html << R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>VeloZ Strategy Comparison Report</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; margin: 20px; background: #f5f7fa; }
        .container { max-width: 1400px; margin: 0 auto; }
        h1 { color: #2c3e50; }
        table { width: 100%; border-collapse: collapse; background: white; margin: 20px 0; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        th, td { padding: 12px; text-align: left; border-bottom: 1px solid #eee; }
        th { background: #2c3e50; color: white; }
        tr:hover { background: #f8f9fa; }
        .positive { color: #27ae60; }
        .negative { color: #e74c3c; }
        .best { background: #d5f5e3; }
        .worst { background: #fadbd8; }
    </style>
</head>
<body>
<div class="container">
    <h1>Strategy Comparison Report</h1>
    <p>Comparing )"
       << results.size() << R"( strategies</p>
    <table>
        <tr>
            <th>Metric</th>)";

  // Add column headers for each strategy
  for (const auto& result : results) {
    html << "<th>" << result.strategy_name.cStr() << "</th>";
  }
  html << "</tr>\n";

  // Helper to find best/worst values
  auto find_best_worst = [&](auto getter, bool higher_is_better) {
    double best_val = getter(results[0]);
    double worst_val = getter(results[0]);
    for (const auto& r : results) {
      double val = getter(r);
      if (higher_is_better) {
        if (val > best_val)
          best_val = val;
        if (val < worst_val)
          worst_val = val;
      } else {
        if (val < best_val)
          best_val = val;
        if (val > worst_val)
          worst_val = val;
      }
    }
    return std::make_pair(best_val, worst_val);
  };

  // Add comparison rows
  auto add_row = [&](const char* metric, auto getter, bool higher_is_better,
                     bool is_percent = false, int precision = 2) {
    auto [best, worst] = find_best_worst(getter, higher_is_better);
    html << "<tr><td>" << metric << "</td>";
    for (const auto& r : results) {
      double val = getter(r);
      std::string css_class;
      if (val == best)
        css_class = "best";
      else if (val == worst)
        css_class = "worst";

      html << "<td class=\"" << css_class << "\">" << std::fixed << std::setprecision(precision)
           << (is_percent ? val * 100 : val) << (is_percent ? "%" : "") << "</td>";
    }
    html << "</tr>\n";
  };

  add_row("Total Return", [](const BacktestResult& r) { return r.total_return; }, true, true);
  add_row("Max Drawdown", [](const BacktestResult& r) { return r.max_drawdown; }, false, true);
  add_row("Sharpe Ratio", [](const BacktestResult& r) { return r.sharpe_ratio; }, true, false, 3);
  add_row("Win Rate", [](const BacktestResult& r) { return r.win_rate; }, true, true);
  add_row("Profit Factor", [](const BacktestResult& r) { return r.profit_factor; }, true, false, 3);
  add_row(
      "Trade Count", [](const BacktestResult& r) { return static_cast<double>(r.trade_count); },
      false, false, 0);
  add_row("Final Balance", [](const BacktestResult& r) { return r.final_balance; }, true, false);
  add_row("Avg Win", [](const BacktestResult& r) { return r.avg_win; }, true, false);
  add_row("Avg Loss", [](const BacktestResult& r) { return r.avg_lose; }, false, false);

  html << R"(    </table>
</div>
</body>
</html>)";

  KJ_IF_SOME(exception, kj::runCatchingExceptions([&]() {
               std::ofstream out_file(output_path.cStr());
               if (!out_file.is_open()) {
                 impl_->logger->error(
                     kj::str("Failed to open file for writing: ", output_path).cStr());
                 return;
               }
               out_file << html.str();
               out_file.close();
               impl_->logger->info(kj::str("Comparison report generated: ", output_path).cStr());
             })) {
    impl_->logger->error(
        kj::str("Failed to write comparison report: ", exception.getDescription()).cStr());
    return false;
  }

  return true;
}

kj::String
BacktestReporter::generate_monthly_returns_html(const kj::Vector<MonthlyReturn>& monthly_returns) {
  std::ostringstream html;

  html << R"(<div class="section">
    <h2>Monthly Returns</h2>
    <div class="table-container">
        <table>
            <tr>
                <th>Year</th>
                <th>Month</th>
                <th>Return</th>
                <th>Trades</th>
                <th>Max Drawdown</th>
            </tr>)";

  for (const auto& mr : monthly_returns) {
    html << "<tr><td>" << mr.year << "</td><td>" << mr.month << "</td><td class=\""
         << (mr.return_pct >= 0 ? "positive" : "negative") << "\">" << std::fixed
         << std::setprecision(2) << (mr.return_pct * 100) << "%</td><td>" << mr.trade_count
         << "</td><td class=\"negative\">" << (mr.max_drawdown * 100) << "%</td></tr>";
  }

  html << R"(        </table>
    </div>
</div>)";

  return kj::str(html.str().c_str());
}

kj::String BacktestReporter::generate_trade_analysis_html(const TradeAnalysis& analysis) {
  std::ostringstream html;

  html << R"(<div class="section">
    <h2>Trade Analysis</h2>
    <div class="table-container">
        <table>
            <tr><th>Metric</th><th>Value</th></tr>
            <tr><td>Best Trade P&L</td><td class="positive">$)"
       << std::fixed << std::setprecision(2) << analysis.best_trade_pnl << R"(</td></tr>
            <tr><td>Worst Trade P&L</td><td class="negative">$)"
       << analysis.worst_trade_pnl << R"(</td></tr>
            <tr><td>Max Consecutive Wins</td><td>)"
       << analysis.max_consecutive_wins << R"(</td></tr>
            <tr><td>Max Consecutive Losses</td><td>)"
       << analysis.max_consecutive_losses << R"(</td></tr>
            <tr><td>Avg Trade Duration</td><td>)"
       << std::setprecision(0) << (analysis.avg_trade_duration_ms / 1000.0) << R"(s</td></tr>
            <tr><td>Avg Winning Duration</td><td>)"
       << (analysis.avg_winning_duration_ms / 1000.0) << R"(s</td></tr>
            <tr><td>Avg Losing Duration</td><td>)"
       << (analysis.avg_losing_duration_ms / 1000.0) << R"(s</td></tr>
        </table>
    </div>
</div>)";

  return kj::str(html.str().c_str());
}

kj::String BacktestReporter::generate_extended_metrics_html(const ExtendedRiskMetrics& metrics) {
  std::ostringstream html;

  html << R"(<div class="section">
    <h2>Extended Risk Metrics</h2>
    <div class="table-container">
        <table>
            <tr><th>Metric</th><th>Value</th></tr>
            <tr><td>Sortino Ratio</td><td>)"
       << std::fixed << std::setprecision(3) << metrics.sortino_ratio << R"(</td></tr>
            <tr><td>Calmar Ratio</td><td>)"
       << metrics.calmar_ratio << R"(</td></tr>
            <tr><td>Omega Ratio</td><td>)"
       << metrics.omega_ratio << R"(</td></tr>
            <tr><td>Tail Ratio</td><td>)"
       << metrics.tail_ratio << R"(</td></tr>
            <tr><td>VaR (95%)</td><td class="negative">)"
       << std::setprecision(2) << (metrics.value_at_risk_95 * 100) << R"(%</td></tr>
            <tr><td>Expected Shortfall (95%)</td><td class="negative">)"
       << (metrics.expected_shortfall_95 * 100) << R"(%</td></tr>
            <tr><td>Skewness</td><td>)"
       << std::setprecision(3) << metrics.skewness << R"(</td></tr>
            <tr><td>Kurtosis</td><td>)"
       << metrics.kurtosis << R"(</td></tr>
            <tr><td>Recovery Factor</td><td>)"
       << metrics.recovery_factor << R"(</td></tr>
            <tr><td>Ulcer Index</td><td>)"
       << metrics.ulcer_index << R"(</td></tr>
        </table>
    </div>
</div>)";

  return kj::str(html.str().c_str());
}

} // namespace veloz::backtest
