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
	int num_row = close.dates_.size();
	int num_col = close.names_.size();
	Eigen::ArrayXXd curr;
	if constexpr (Freq == LongshortFrequency::INTRADAY) {
		auto open = load_ohlc(directory, "op");
		curr = open.values_.bottomRows(num_row - 1).array();
	} else {
		curr = close.values_.bottomRows(num_row - 1).array();
	}
	const auto& prev = close.values_.topRows(num_row - 1).array();
	Eigen::ArrayXXd pct_change = (curr - prev) / prev;
	Eigen::ArrayXd cl_ret_mean = pct_change.rowwise().mean();
	Eigen::ArrayXXd weights = -(pct_change.colwise() - cl_ret_mean);
	weights.colwise() /= weights.abs().rowwise().sum();
	Eigen::ArrayXd ret = (weights * pct_change).rowwise().sum();
	ret = ret.isNaN().select(0.0, ret);
	double apr = std::pow((1 + ret).prod(), 252.0 / ret.size()) - 1;
	double ret_mean = ret.mean();
	double sharpe = ret_mean * std::sqrt(252.0 * (ret.size() - 1.0)) / (ret - ret_mean).matrix().norm();
	std::cout << "apr=" << apr << ", sharpe=" << sharpe << std::endl;
	return 0;
}

} // namespace strategy_replications