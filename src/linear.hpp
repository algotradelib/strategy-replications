#pragma once

#include <strategy_replications/src/reversion/linear_reversion.hpp>
#include "./backtesting.hpp"
#include "./utils.hpp"

#include <string>

namespace strategy_replications {

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

} // namespace strategy_replications