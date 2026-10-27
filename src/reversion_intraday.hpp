#pragma once

#include <strategy_replications/reversion_intraday>
#include "./backtesting_intraday.hpp"
// #include "./utils.hpp"

#include <string>

namespace strategy_replications {

// Example 4.1 Intraday Mean Reversion
int buy_on_gap(const std::string& directory) {
	auto open = load_ohlc(directory, "op");
	auto high = load_ohlc(directory, "hi");
	auto low = load_ohlc(directory, "lo");
	auto close = load_ohlc(directory, "cl");
	reversion::BuyOnGap bog_model;
	bog_model.params_.universe_ = {};
	for (const auto& name : close.names_) {
		bog_model.params_.universe_.push_back(utils::id_of(name));
	}
	backtesting::Config configs;
	configs.cash_ = "2000000000";
	const double capital = bog_model.params_.capital_; // read before the move
	const auto engine = backtesting::replay_intraday(std::move(bog_model), open, high, low, close, configs);
	const auto& result = engine.result();
	backtesting::replay_log(bog_model.params_.tag_, result);
	backtesting::summarize_intraday(close, result, capital, bog_model.params_.window_ + 1);
  return 0;
}

// Example 4.4
int longshort_intraday(const std::string& directory) {
	auto open = load_ohlc(directory, "op");
	auto high = load_ohlc(directory, "hi");
	auto low = load_ohlc(directory, "lo");
	auto close = load_ohlc(directory, "cl");
	open.slice_dates(20070103, 20111230);
	high.slice_dates(20070103, 20111230);
	low.slice_dates(20070103, 20111230);
	close.slice_dates(20070103, 20111230);
	reversion::IntradayLongShort longshort_model;
	longshort_model.params_.universe_ = {};
	for (const auto& name : close.names_) {
		longshort_model.params_.universe_.push_back(utils::id_of(name));
	}
	backtesting::Config configs;
	configs.cash_ = "2000000000";
	const double capital = longshort_model.params_.capital_;
	const auto engine = backtesting::replay_intraday(std::move(longshort_model), open, high, low, close, configs);
	const auto& result = engine.result();
	backtesting::replay_log(longshort_model.params_.tag_, result);
	backtesting::summarize_intraday(close, result, capital, 1);
  return 0;
}

} // namespace strategy_replications