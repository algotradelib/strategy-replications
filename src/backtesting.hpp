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

inline backtest::ReplayConfig config_for(const std::vector<std::string>& names, const Config& configs) {
	backtest::ReplayConfig replay;
	replay.cash_ = Decimal64::parse(configs.cash_);
	replay.currency_ = Currency{"USD"};
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

backtest::NextBarFillModel costs_none() {
  return backtest::NextBarFillModel(
    backtest::CommissionModel::none(),
    backtest::SlippageModel::none()
  );
}

template <typename Strategy>
inline backtest::Backtester<Strategy> replay(Strategy strategy, TimeSeriesData& data, const std::vector<std::string>& names, const Config& configs) {
  backtest::Backtester<Strategy> engine(std::move(strategy), config_for(names, configs), costs_none());
  std::vector<std::size_t> columns;
  for (const auto& name : names) {
		columns.push_back(data.col(name));
	}
  std::vector<std::optional<Decimal64>> previous(names.size());
  for (std::size_t day = 0; day < data.dates_.size(); ++day) {
    for (std::size_t at = 0; at < names.size(); ++at) {
			// const auto& text = data.cell_[data.col(names[at])][day];
      const auto& text = data.cell_[columns[at]][day];
      if (text.empty()) {
				continue;
			}
      auto bar = utils::bar_of(data.dates_[day], text);
			// Each bar opens at the previous close, so a market order decided at close t fills at close t (the book's convention),
			// and summarize counts it as held from that close.
      if (previous[at]) {
        bar.open_ = *previous[at];
        if (atl::decimal_less(*previous[at], bar.low_)) {
					bar.low_ = *previous[at];
				}
        if (atl::decimal_less(bar.high_, *previous[at])) {
					bar.high_ = *previous[at];
				}
      }
      previous[at] = bar.close_;
      engine.feed(strategy::make_bar_event(utils::id_of(names[at]), BarInterval::ONE_DAY, BarStamp::CLOSE, bar));
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
	int num_col = data.names_.size();
	std::map<std::string, int> asset_idx;
  // for (int i = 0; i < num_col; ++i) {
  //   asset_idx[names[i]] = i;
  // }
	for (const auto& name : names) {
		asset_idx[name] = static_cast<int>(data.col(name)); // to follow the csv order, not names
	}
	Eigen::ArrayXXd positions = Eigen::ArrayXXd::Zero(num_row, num_col);
	std::vector<double> running(num_col, 0.0);
	// std::vector<double> prices(num_col, 0.0);
	int next = 0;
	for (int day = 0; day < num_row; ++day) {
		const auto close = day + 1 < num_row ? utils::days_from_civil(data.dates_[day + 1]) : utils::days_from_civil(data.dates_[day]);
		while (next < result.executions_.size() && !(close < result.executions_[next].fill_.at_)) {
			const auto& fill = result.executions_[next].fill_;
			const double quantity = std::stod(fill.quantity_.str());
			running[asset_idx.at(fill.instrument_.symbol_)] += fill.side_ == OrderSide::BUY ? quantity : -quantity;
			// prices[asset_idx.at(fill.instrument_.symbol_)] = analytics::to_scalar(fill.price_);
			++next;
		}
		for (int at = 0; at < num_col; ++at) {
			// positions(day, at) = running[at] * prices[at];
			positions(day, at) = running[at];
		}
	}
	positions *= data.values_.array();
	Eigen::MatrixXd pct_change = math::unobserved(num_row - 1, num_col);
	const auto& curr = data.values_.bottomRows(num_row - 1).array();
	const auto& prev = data.values_.topRows(num_row - 1).array();
	const auto& position_prev = positions.topRows(num_row - 1);
	pct_change = (curr - prev) / prev; // (num_row - 1) x num_col
	// Eigen::MatrixXd pnl = prev * pct_change.array();
	// Eigen::MatrixXd ret = pnl.array() / prev.cwiseAbs();
	Eigen::ArrayXd pnl = (position_prev * pct_change.array()).rowwise().sum();
	Eigen::ArrayXd ret = pnl / position_prev.cwiseAbs().rowwise().sum();
	ret = ret.isNaN().select(0.0, ret);
	double apr = std::pow((1 + ret).prod(), 252.0 / ret.size()) - 1;
	double ret_mean = ret.mean();
	// double ret_sd = std::sqrt((ret - ret_mean).matrix().norm() / (ret.size() - 1.0));
	double sharpe = ret_mean * std::sqrt(252.0 * (ret.size() - 1.0)) / (ret - ret_mean).matrix().norm();
	std::cout << "apr=" << apr << ", sharpe=" << sharpe << std::endl;
}

} // namespace backtesting
} // namespace strategy_replications