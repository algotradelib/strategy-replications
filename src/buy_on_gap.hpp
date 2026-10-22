#pragma once

#include "./data.hpp"

#include <string>
#include <numeric>
#include <algorithm>

namespace strategy_replications {

namespace detail {

inline TimeSeriesData load_ohlc(const std::string& directory, const std::string& kind) {
	auto df = load_csv(directory + "/inputDataOHLCDaily_20120424_" + kind + ".csv");
	std::ifstream file(directory + "/inputDataOHLCDaily_20120424_stocks.csv");
	std::string line;
	std::getline(file, line);
	std::getline(file, line);
	std::vector<std::string> names;
	std::size_t from = 0;
	while (true) {
		const auto to = line.find(',', from);
		auto cell = line.substr(from, to == std::string::npos ? std::string::npos : to - from);
		while (!cell.empty() && (cell.back() == '\r' || cell.back() == ' ')) {
			cell.pop_back();
		}
		names.push_back(cell);
		if (to == std::string::npos) {
			break;
		}
		from = to + 1;
	}
	df.names_ = names;
	return df;
}

} // namespace detail

int buy_on_gap(const std::string& directory) {
	int top = 10;
	int lookback = 20;
	int window = 90;
	double entry = 1.0;
	auto open = detail::load_ohlc(directory, "op");
	auto high = detail::load_ohlc(directory, "hi");
	auto low = detail::load_ohlc(directory, "lo");
	auto close = detail::load_ohlc(directory, "cl");
	int num_row = close.dates_.size();
	int num_col = close.names_.size();
	int t_start = std::max(window + 1, lookback);
	const auto& curr = close.values_.bottomRows(num_row - 1).array();
	const auto& prev = close.values_.topRows(num_row - 1).array();
	Eigen::ArrayXXd pct_change = (curr - prev) / prev;
	Eigen::ArrayXd cl_ret_sum = pct_change.topRows(window).colwise().sum();
	Eigen::ArrayXd cl_ret_ss = pct_change.topRows(window).colwise().squaredNorm();
	// Eigen::ArrayXd cl_ret_sd = cl_ret_ss - (cl_ret_sum / window).square();
	Eigen::ArrayXd cl_ret_sd(num_col);
	Eigen::ArrayXd ret_gap(num_col);
	Eigen::VectorXi ret_id(num_col);
	Eigen::ArrayXd curr_open(num_col);
	Eigen::ArrayXd prev_low(num_col);
	// Eigen::ArrayXd cl_ma(num_col);
	Eigen::ArrayXd cl_sum = close.values_.middleRows(t_start - lookback, lookback).colwise().sum();
	Eigen::Array<bool, Eigen::Dynamic, 1> gapped(num_col);
	// Eigen::Array<bool, Eigen::Dynamic, Eigen::Dynamic> positions = Eigen::Array<bool, Eigen::Dynamic, Eigen::Dynamic>::Constant(num_row, num_col, false);
	Eigen::ArrayXXd positions = Eigen::ArrayXXd::Zero(num_row - t_start, num_col);
	int num_gapped = 0;
	for (int i = t_start; i < num_row; ++i) {
		cl_ret_sum += pct_change.row(i - 1).array() - pct_change.row(i - 1 - window).array();
		cl_ret_ss += pct_change.row(i - 1).array().square() - pct_change.row(i - 1 - window).array().square();
		cl_ret_sd = ((cl_ret_ss - cl_ret_sum.square() / window) / (window - 1)).sqrt();
		// cl_ma = close.values_.middleRows(i - lookback, lookback).colwise().mean();
		cl_sum += close.values_.row(i).array() - close.values_.row(i - lookback).array();
		curr_open = open.values_.row(i).array();
		prev_low = low.values_.row(i - 1).array();
		// ret_gap = (open.values_.row(i).array() - low.values_.row(i - 1).array()) / low.values_.row(i - 1).array();
		ret_gap = (curr_open - prev_low) / prev_low;
		// gapped = (curr_open < prev_low * (1 - entry * cl_ret_sd)) * (curr_open > cl_sum / window);
		gapped = ret_gap.isFinite() * (curr_open < prev_low * (1 - entry * cl_ret_sd)) * (curr_open > cl_sum / lookback);
		num_gapped = gapped.count();
		if (num_gapped > top) {
			std::iota(ret_id.data(), ret_id.data() + ret_id.size(), 0);
			std::nth_element(ret_id.data(), ret_id.data() + top, ret_id.data() + ret_id.size(),
				[&gapped, &ret_gap](int i1, int i2) {
					const bool gapped1 = gapped(i1);
					const bool gapped2 = gapped(i2);
					if (gapped1 != gapped2) {
						return gapped1;
					}
					return gapped1 && ret_gap(i1) > ret_gap(i2);
				}
			);
			positions.row(i - t_start)(ret_id(Eigen::seqN(0, top))) = 1.0;
		} else if (num_gapped > 0) {
			positions.row(i - t_start).array() += gapped.cast<double>();
		}
	}
	Eigen::ArrayXXd ret_oc = ((close.values_.array() - open.values_.array()) / open.values_.array()).bottomRows(num_row - t_start);
	ret_oc = ret_oc.isNaN().select(0.0, ret_oc);
	Eigen::ArrayXd pnl = (positions * ret_oc).rowwise().sum();
	Eigen::ArrayXd ret = pnl / top;
	// ret = ret.isNaN().select(0.0, ret);
	double apr = std::pow((1 + ret).prod(), 252.0 / ret.size()) - 1;
	double ret_mean = ret.mean();
	double sharpe = ret_mean * std::sqrt(252.0 * (ret.size() - 1.0)) / (ret - ret_mean).matrix().norm();
	std::cout << "apr=" << apr << ", sharpe=" << sharpe << std::endl;
	return 0;
}

} // namespace strategy_replications