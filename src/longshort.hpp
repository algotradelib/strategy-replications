#pragma once

#include "./data.hpp"

#include <iostream>
#include <string>
#include <numeric>
#include <algorithm>

namespace strategy_replications {

enum class LongshortFrequency {
	DAILY, // example 4.3
	INTRADAY, // example 4.4
};

template <LongshortFrequency Freq = LongshortFrequency::DAILY>
int longshort(const std::string& directory) {
	auto close = load_ohlc(directory, "cl");
	close.slice_dates(20070103, 20111230);
	int num_row = close.dates_.size();
	int num_col = close.names_.size();
	TimeSeriesData open;
	Eigen::ArrayXXd curr;
	if constexpr (Freq == LongshortFrequency::INTRADAY) {
		open = load_ohlc(directory, "op");
		open.slice_dates(20070103, 20111230);
		curr = open.values_.bottomRows(num_row - 1).array();
	} else {
		curr = close.values_.bottomRows(num_row - 1).array();
	}
	const auto& prev = close.values_.topRows(num_row - 1).array();
	Eigen::ArrayXXd pct_change = (curr - prev) / prev;
	// pct_change includes nan
	Eigen::ArrayXd n_valid_row = pct_change.isFinite().cast<double>().rowwise().sum();
	Eigen::ArrayXd cl_ret_mean = pct_change.isFinite().select(pct_change, 0.0).rowwise().sum() / n_valid_row;
	Eigen::ArrayXXd weights = -(pct_change.colwise() - cl_ret_mean);
	weights.colwise() /= weights.isFinite().select(weights, 0.0).abs().rowwise().sum();
	Eigen::ArrayXXd pnl;
	if constexpr (Freq == LongshortFrequency::INTRADAY) {
		pnl = weights * ((close.values_.array() - open.values_.array()) / open.values_.array()).bottomRows(num_row - 1);
	} else {
		pnl = weights.topRows(num_row - 2) * pct_change.bottomRows(num_row - 2);
	}
	Eigen::ArrayXd ret = pnl.isFinite().select(pnl, 0.0).rowwise().sum();
	ret = ret.isNaN().select(0.0, ret);
	double apr = std::pow((1 + ret).prod(), 252.0 / ret.size()) - 1;
	double ret_mean = ret.mean();
	double sharpe = ret_mean * std::sqrt(252.0 * (ret.size() - 1.0)) / (ret - ret_mean).matrix().norm();
	std::cout << "apr=" << apr << ", sharpe=" << sharpe << std::endl;
	return 0;
}

} // namespace strategy_replications