#pragma once

#include <strategy_replications/src/reversion/bollinger_band.hpp>
#include "./backtesting.hpp"
#include "./utils.hpp"

#include <string>

namespace strategy_replications {

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

} // namespace strategy_replications