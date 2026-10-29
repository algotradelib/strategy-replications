#pragma once

#include "./data.hpp"

#include <string>
#include <numeric>
#include <algorithm>

namespace strategy_replications {

// Example 4.1
int bog_vectorized(const std::string& directory) {
	int top_ = 10;
	int lookback_ = 20;
	int window_ = 90;
	double entry_ = 1.0;
	auto open = load_ohlc(directory, "op");
	auto high = load_ohlc(directory, "hi");
	auto low = load_ohlc(directory, "lo");
	auto close = load_ohlc(directory, "cl");
	int num_row = close.dates_.size();
	int num_col = close.names_.size();
	int t_start = std::max(window_ + 1, lookback_);
	const auto& curr = close.values_.bottomRows(num_row - 1).array();
	const auto& prev = close.values_.topRows(num_row - 1).array();
	// Eigen::ArrayXXd pct_change = (curr - prev) / prev;
	Eigen::ArrayXXd pct_change = Eigen::ArrayXXd::Zero(num_row, num_col);
	pct_change.bottomRows(num_row - 1) = (curr - prev) / prev;
	const Eigen::ArrayXXd pct_count = pct_change.isFinite().cast<double>();
	pct_change = pct_change.isFinite().select(pct_change, 0.0);
	const Eigen::ArrayXXd cl_count_all = close.values_.array().isFinite().cast<double>();
	const Eigen::ArrayXXd cl_values = close.values_.array().isFinite().select(close.values_.array(), 0.0);
	Eigen::ArrayXd cl_ret_sum = pct_change.topRows(window_).colwise().sum();
	Eigen::ArrayXd cl_ret_ss = pct_change.topRows(window_).colwise().squaredNorm();
	Eigen::ArrayXd cl_ret_count = pct_count.topRows(window_).colwise().sum();
	Eigen::ArrayXd cl_ret_sd(num_col);
	Eigen::ArrayXd ret_gap(num_col);
	Eigen::VectorXi ret_id(num_col);
	Eigen::ArrayXd curr_open(num_col);
	Eigen::ArrayXd prev_low(num_col);
	Eigen::ArrayXd cl_sum = cl_values.middleRows(t_start - lookback_ - 1, lookback_).colwise().sum();
	Eigen::ArrayXd cl_count = cl_count_all.middleRows(t_start - lookback_ - 1, lookback_).colwise().sum();
	Eigen::Array<bool, Eigen::Dynamic, 1> gapped(num_col);
	// Eigen::Array<bool, Eigen::Dynamic, Eigen::Dynamic> positions = Eigen::Array<bool, Eigen::Dynamic, Eigen::Dynamic>::Constant(num_row, num_col, false);
	Eigen::ArrayXXd positions = Eigen::ArrayXXd::Zero(num_row - t_start, num_col);
	int num_gapped = 0;
	for (int i = t_start; i < num_row; ++i) {
		cl_ret_sum += pct_change.row(i - 1).array() - pct_change.row(i - 1 - window_).array();
		cl_ret_ss += pct_change.row(i - 1).array().square() - pct_change.row(i - 1 - window_).array().square();
		cl_ret_count += pct_count.row(i - 1) - pct_count.row(i - 1 - window_);
		cl_ret_sd = ((cl_ret_ss - cl_ret_sum.square() / cl_ret_count) / (cl_ret_count - 1)).sqrt();
		cl_sum += cl_values.row(i - 1) - cl_values.row(i - 1 - lookback_);
		cl_count += cl_count_all.row(i - 1) - cl_count_all.row(i - 1 - lookback_);
		curr_open = open.values_.row(i).array();
		prev_low = low.values_.row(i - 1).array();
		ret_gap = (curr_open - prev_low) / prev_low;
		gapped = ret_gap.isFinite() * (curr_open < prev_low * (1 - entry_ * cl_ret_sd)) * (curr_open > cl_sum / cl_count);
		num_gapped = gapped.count();
		if (num_gapped > top_) {
			std::iota(std::begin(ret_id), std::begin(ret_id) + ret_id.size(), 0);
			std::nth_element(std::begin(ret_id), std::begin(ret_id) + top_, std::end(ret_id),
				[&gapped, &ret_gap](int i1, int i2) {
					const bool gapped1 = gapped(i1);
					const bool gapped2 = gapped(i2);
					if (gapped1 != gapped2) {
						return gapped1;
					}
					return gapped1 && ret_gap(i1) < ret_gap(i2); // ascending among gap-downed columns
				}
			);
			positions.row(i - t_start)(ret_id(Eigen::seqN(0, top_))) = 1.0;
		} else if (num_gapped > 0) {
			positions.row(i - t_start).array() += gapped.cast<double>();
		}
	}
	Eigen::ArrayXXd ret_oc = ((close.values_.array() - open.values_.array()) / open.values_.array()).bottomRows(num_row - t_start);
	ret_oc = ret_oc.isNaN().select(0.0, ret_oc);
	Eigen::ArrayXd pnl = (positions * ret_oc).rowwise().sum();
	Eigen::ArrayXd ret = pnl / top_;
	double apr = std::pow((1 + ret).prod(), 252.0 / ret.size()) - 1;
	double ret_mean = ret.mean();
	double sharpe = ret_mean * std::sqrt(252.0 * (ret.size() - 1.0)) / (ret - ret_mean).matrix().norm();
	std::cout << "apr=" << apr << ", sharpe=" << sharpe << std::endl;
	return 0;
}

} // namespace strategy_replications