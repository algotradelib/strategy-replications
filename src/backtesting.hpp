#pragma once

#include <algotradelib/analytics>
#include <algotradelib/backtest>
#include <algotradelib/strategy>
#include "./data.hpp"
#include "./utils.hpp"

#include <iostream>
#include <chrono>

namespace strategy_replications {

namespace math = algotradelib::math;
namespace analytics = algotradelib::analytics;
namespace backtest = algotradelib::backtest;
namespace strategy = algotradelib::strategy;

using atl::OrderSide;

namespace backtesting {

struct Config {
	std::string cash_{"100000"};
	bool shorts_{true};
	std::string commission{"0"};
	int trace_{0};
};

// struct Held {
//   std::vector<std::vector<double>> shares; // [name][day]
//   std::size_t executions{0};
//   std::size_t refused{0};
//   std::size_t rejected{0};
//   std::size_t unfilled{0};
//   std::string halted;
//   double final_equity{0};
// };

inline backtest::ReplayConfig config_for(const std::vector<std::string>& names, const Config& configs) {
	backtest::ReplayConfig replay;
	replay.cash_ = Decimal64::parse(configs.cash_);
	for (const auto& name : names) {
		replay.instruments_.push_back(utils::reference(name));
	}
  replay.series_capacity_ = 512;
  replay.limits_.permitted_phases_ = std::nullopt;
  if (configs.shorts_) {
    replay.limits_.enforce_sellable_quantity_ = false;
  }
  return replay;
}

backtest::NextBarFillModel costs() {
  return backtest::NextBarFillModel(
    backtest::CommissionModel::of(Decimal64::parse("0.00015"), Decimal64::parse("100")),
    backtest::SlippageModel::of_ticks(0)
  );
}

template <typename Strategy>
backtest::Backtester<Strategy> replay(Strategy strategy, TimeSeriesData& data, const std::vector<std::string>& names, const Config& configs) {
  backtest::Backtester<Strategy> engine(std::move(strategy), config_for(names, configs), costs());
  std::vector<std::size_t> columns;
  for (const auto& name : names) {
		columns.push_back(data.col(name));
	}
  for (std::size_t day = 0; day < data.dates_.size(); ++day) {
    for (std::size_t at = 0; at < names.size(); ++at) {
			// const auto& text = data.cell_[data.col(names[at])][day];
      const auto& text = data.cell_[columns[at]][day];
      if (text.empty()) {
				continue;
			}
      engine.feed(strategy::make_bar_event(utils::id_of(names[at]), BarInterval::ONE_DAY, BarStamp::CLOSE, utils::bar_of(data.dates_[day], text)));
    }
  }
  engine.finish();
  return engine;
}

inline void replay_log(const std::string& tag, const backtest::ReplayResult& result) {
	std::cout << tag << " replayed: "
		<< result.bars_ << " bars and filled "
		<< result.executions_.size() << " orders."
		<< std::endl;
}

inline void summarize(const TimeSeriesData& data, const std::vector<std::string>& names, const backtest::ReplayResult& result, std::size_t start_day) {
	int num_row = data.dates_.size();
	int num_col = names.size();
	std::map<std::string, int> asset_idx;
  for (int i = 0; i < num_col; ++i) {
    asset_idx[names[i]] = i;
  }
	Eigen::ArrayXXd positions = Eigen::ArrayXXd::Zero(num_row, num_col);
	std::vector<double> running(num_col, 0.0);
	std::vector<double> prices(num_col, 0.0);
	int next = 0;
	for (int day = 0; day < num_row; ++day) {
		const auto close = utils::days_from_civil(data.dates_[day]) + std::chrono::hours(24);
		while (next < result.executions_.size() && !(close < result.executions_[next].fill_.at_)) {
			const auto& fill = result.executions_[next].fill_;
			const double quantity = std::stod(fill.quantity_.str());
			running[asset_idx.at(fill.instrument_.symbol_)] += fill.side_ == OrderSide::BUY ? quantity : -quantity;
			prices[asset_idx.at(fill.instrument_.symbol_)] = analytics::to_scalar(fill.price_);
			++next;
		}
		for (int at = 0; at < num_col; ++at) {
			positions(day, at) = running[at] * prices[at];
			// positions(day, at) = running[at];
		}
	}
	// positions *= data.values_.array();
	std::cout << "values:\n" << data.values_.topRows<5>() << std::endl;
	Eigen::MatrixXd pct_change = math::unobserved(num_row - 1, num_col);
	const auto& curr = data.values_.bottomRows(num_row - 1).array();
	const auto& prev = data.values_.topRows(num_row - 1).array();
	const auto& position_prev = positions.topRows(num_row - 1);
	// pct_change.bottomRows(num_row - 1) = (prev <= 0).select(math::missing(), (curr - prev) / prev);
	pct_change = (curr - prev) / prev; // (num_row - 1) x num_col
	// Eigen::MatrixXd pnl = prev * pct_change.array();
	// Eigen::MatrixXd ret = pnl.array() / prev.cwiseAbs();
	Eigen::ArrayXd pnl = (position_prev * pct_change.array()).rowwise().sum();
	std::cout << "pnl\n" << (position_prev * pct_change.array()).topRows<5>() << "\n"
		<< pnl.topRows<5>() << std::endl;
	Eigen::ArrayXd ret = pnl / position_prev.cwiseAbs().rowwise().sum();
	double apr = std::pow((1 + ret).prod(), 252 / ret.size()) - 1;
	double ret_mean = ret.mean();
	// double ret_sd = std::sqrt((ret - ret_mean).matrix().norm() / (ret.size() - 1));
	double sharpe = ret_mean * std::sqrt(252 * (ret.size() - 1) / (ret - ret_mean).matrix().norm());
	std::cout << "apr=" << apr << ", sharpe=" << sharpe << std::endl;
}

} // namespace backtesting
} // namespace strategy_replications