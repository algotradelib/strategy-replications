#pragma once

#include <strategy_replications/src/reversion/kalman_reversion.hpp>
#include "./backtesting.hpp"
#include "./utils.hpp"

#include <string>

namespace strategy_replications {

int kalman(const std::string& path, const std::vector<std::string>& names, bool shorts) {
	auto df = load_csv(path);
  reversion::KalmanReversion kalman_reversion;
	kalman_reversion.params_.scale_ = 1;
	kalman_reversion.params_.instrument_y_ = utils::id_of(names[0]);
	kalman_reversion.params_.instrument_x_ = utils::id_of(names[1]);
	backtesting::Config configs;
	configs.shorts_ = shorts;
	// const std::vector<std::string> names{x, y};
	const auto engine = backtesting::replay(std::move(kalman_reversion), df, names, configs);
	const auto& result = engine.result();
	backtesting::replay_log(kalman_reversion.params_.tag_, result);
	backtesting::summarize(df, names, result, 0);
  return 0;
}

} // namespace strategy_replications