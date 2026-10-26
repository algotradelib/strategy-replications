#pragma once

#include <strategy_replications/reversion>
#include "./backtesting.hpp"
#include "./utils.hpp"

#include <string>

namespace strategy_replications {

// Example 3.1 Trading Price Spread, Log Price Spread, and Ratio
template <reversion::LinearSpread Spread = reversion::LinearSpread::PRICE>
int linear(const std::string& path, const std::vector<std::string>& names, bool shorts) {
	auto df = load_csv(path);
  reversion::LinearReversion<Spread> linear_reversion;
	linear_reversion.params_.scale_ = (Spread == reversion::LinearSpread::PRICE) ? 100000 : 10000000;
	linear_reversion.params_.instrument_y_ = utils::id_of(names[0]);
	linear_reversion.params_.instrument_x_ = utils::id_of(names[1]);
	backtesting::Config configs;
	configs.shorts_ = shorts;
	const auto engine = backtesting::replay(std::move(linear_reversion), df, names, configs);
	const auto& result = engine.result();
	backtesting::replay_log(linear_reversion.params_.tag_, result);
	backtesting::summarize(df, names, result, 0);
  return 0;
}

// Example 3.2 Bollinger Band Mean Reversion Strategy
int bollinger(const std::string& path, const std::vector<std::string>& names, bool shorts) {
	auto df = load_csv(path);
	reversion::BollingerBand bollinger_band;
	bollinger_band.params_.scale_ = 1000;
	bollinger_band.params_.instrument_y_ = utils::id_of(names[0]);
	bollinger_band.params_.instrument_x_ = utils::id_of(names[1]);
	backtesting::Config configs;
	configs.shorts_ = shorts;
	const auto engine = backtesting::replay(std::move(bollinger_band), df, names, configs);
	const auto& result = engine.result();
	backtesting::replay_log(bollinger_band.params_.tag_, result);
	backtesting::summarize(df, names, result, 0);
  return 0;
}

// Example 3.3 Kalman Filter Mean Reversion Strategy
int kalman(const std::string& path, const std::vector<std::string>& names, bool shorts) {
	auto df = load_csv(path);
  reversion::KalmanReversion kalman_reversion;
	kalman_reversion.params_.scale_ = 1000;
	kalman_reversion.params_.instrument_y_ = utils::id_of(names[0]);
	kalman_reversion.params_.instrument_x_ = utils::id_of(names[1]);
	backtesting::Config configs;
	configs.shorts_ = shorts;
	const auto engine = backtesting::replay(std::move(kalman_reversion), df, names, configs);
	const auto& result = engine.result();
	backtesting::replay_log(kalman_reversion.params_.tag_, result);
	backtesting::summarize(df, names, result, 0);
  return 0;
}

// Example 4.3 Linear Long-Short Model
int longshort_daily(const std::string& directory) {
	auto df = load_ohlc(directory, "cl");
	df.slice_dates(20070103, 20111230);
  reversion::LinearLongShort linear_longshort;
	linear_longshort.params_.scale_ = 100000;
	linear_longshort.params_.universe_ = {};
	for (const auto& name : df.names_) {
		linear_longshort.params_.universe_.push_back(utils::id_of(name));
	}
	backtesting::Config configs;
	const auto engine = backtesting::replay(std::move(linear_longshort), df, df.names_, configs);
	const auto& result = engine.result();
	backtesting::replay_log(linear_longshort.params_.tag_, result);
	backtesting::summarize(df, df.names_, result, 1);
  return 0;
}

} // namespace strategy_replications