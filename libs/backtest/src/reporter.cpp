#include "veloz/backtest/reporter.h"
#include "veloz/core/logger.h"
#include <iostream>
#include <fstream>
#include <sstream>

namespace veloz::backtest {

struct BacktestReporter::Impl {
    std::shared_ptr<veloz::core::Logger> logger;

    Impl() {
        logger = std::make_shared<veloz::core::Logger>(std::cout);
    }
};

BacktestReporter::BacktestReporter() : impl_(std::make_unique<Impl>()) {}

BacktestReporter::~BacktestReporter() {}

bool BacktestReporter::generate_report(const BacktestResult& result, const std::string& output_path) {
    impl_->logger->info(std::format("Generating report to: {}", output_path));

    // TODO: Implement report generation
    // For now, just create a simple HTML file

    std::string html_content = generate_html_report(result);

    // Write to file
    try {
        std::ofstream out_file(output_path);
        if (!out_file.is_open()) {
            impl_->logger->error(std::format("Failed to open file: {}", output_path));
            return false;
        }

        out_file << html_content;
        out_file.close();

        impl_->logger->info(std::format("Report generated successfully: {}", output_path));
        return true;
    } catch (const std::exception& e) {
        impl_->logger->error(std::format("Failed to write report: {}", e.what()));
        return false;
    }
}

std::string BacktestReporter::generate_html_report(const BacktestResult& result) {
    std::stringstream html;

    html << R"(
        <!DOCTYPE html>
        <html lang="zh-CN">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>VeloZ 回测报告</title>
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
                    background-color: #f8f9fa;
                    border-radius: 4px;
                    display: flex;
                    align-items: center;
                    justify-content: center;
                }

                .chart-placeholder {
                    text-align: center;
                    color: #7f8c8d;
                }
            </style>
        </head>
        <body>
            <div class="container">
                <div class="header">
                    <h1>VeloZ 回测报告</h1>
                    <p>策略名称: )" << result.strategy_name << R"( | 交易对: )" << result.symbol << R"(</p>
                    <p>回测时间: )" << result.start_time << R"( - )" << result.end_time << R"(</p>
                </div>

                <div class="summary">
                    <div class="stat-card">
                        <h3>初始资金</h3>
                        <div class="value">$)" << result.initial_balance << R"(</div>
                    </div>

                    <div class="stat-card">
                        <h3>最终资金</h3>
                        <div class="value">$)" << result.final_balance << R"(</div>
                    </div>

                    <div class="stat-card">
                        <h3>总收益率</h3>
                        <div class="value )" << (result.total_return >= 0 ? "positive" : "negative") << R"()">)" << (result.total_return * 100) << R"(%</div>
                    </div>

                    <div class="stat-card">
                        <h3>最大回撤</h3>
                        <div class="value )" << (result.max_drawdown >= 0 ? "negative" : "positive") << R"()">)" << (result.max_drawdown * 100) << R"(%</div>
                    </div>

                    <div class="stat-card">
                        <h3>夏普比率</h3>
                        <div class="value">)" << result.sharpe_ratio << R"()</div>
                    </div>

                    <div class="stat-card">
                        <h3>胜率</h3>
                        <div class="value )" << (result.win_rate >= 0.5 ? "positive" : "negative") << R"()">)" << (result.win_rate * 100) << R"(%</div>
                    </div>
                </div>

                <div class="content">
                    <div class="section">
                        <h2>详细结果</h2>

                        <div class="table-container">
                            <table>
                                <tr>
                                    <th>指标</th>
                                    <th>数值</th>
                                </tr>
                                <tr>
                                    <td>总交易次数</td>
                                    <td>)" << result.trade_count << R"()</td>
                                </tr>
                                <tr>
                                    <td>盈利交易次数</td>
                                    <td>)" << result.win_count << R"()</td>
                                </tr>
                                <tr>
                                    <td>亏损交易次数</td>
                                    <td>)" << result.lose_count << R"()</td>
                                </tr>
                                <tr>
                                    <td>盈亏比</td>
                                    <td>)" << result.profit_factor << R"()</td>
                                </tr>
                                <tr>
                                    <td>平均盈利</td>
                                    <td>$)" << result.avg_win << R"()</td>
                                </tr>
                                <tr>
                                    <td>平均亏损</td>
                                    <td class=")" << (result.avg_lose < 0 ? "negative" : "positive") << R"()">$)" << result.avg_lose << R"()</td>
                                </tr>
                            </table>
                        </div>
                    </div>

                    <div class="section">
                        <h2>权益曲线</h2>
                        <div class="chart-container">
                            <div class="chart-placeholder">
                                <p>图表功能开发中...</p>
                            </div>
                        </div>
                    </div>

                    <div class="section">
                        <h2>回撤曲线</h2>
                        <div class="chart-container">
                            <div class="chart-placeholder">
                                <p>图表功能开发中...</p>
                            </div>
                        </div>
                    </div>

                    <div class="section">
                        <h2>交易记录</h2>
                        <div class="table-container">
                            <table>
                                <tr>
                                    <th>时间</th>
                                    <th>交易对</th>
                                    <th>方向</th>
                                    <th>价格</th>
                                    <th>数量</th>
                                    <th>费用</th>
                                    <th>盈亏</th>
                                </tr>
    )";

    // TODO: Add trade records to table

    html << R"(
                            </table>
                        </div>
                    </div>
                </div>
            </div>
        </body>
        </html>
    )";

    return html.str();
}

std::string BacktestReporter::generate_json_report(const BacktestResult& result) {
    std::stringstream json;

    json << R"({
        "strategy_name": ")" << result.strategy_name << R"(",
        "symbol": ")" << result.symbol << R"(",
        "start_time": )" << result.start_time << R"(,
        "end_time": )" << result.end_time << R"(,
        "initial_balance": )" << result.initial_balance << R"(,
        "final_balance": )" << result.final_balance << R"(,
        "total_return": )" << result.total_return << R"(,
        "max_drawdown": )" << result.max_drawdown << R"(,
        "sharpe_ratio": )" << result.sharpe_ratio << R"(,
        "win_rate": )" << result.win_rate << R"(,
        "profit_factor": )" << result.profit_factor << R"(,
        "trade_count": )" << result.trade_count << R"(,
        "win_count": )" << result.win_count << R"(,
        "lose_count": )" << result.lose_count << R"(,
        "avg_win": )" << result.avg_win << R"(,
        "avg_lose": )" << result.avg_lose << R"(,
        "trades": [)";

    // TODO: Add trade records

    json << R"(
        ]
    })";

    return json.str();
}

} // namespace veloz::backtest
