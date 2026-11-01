#pragma once

#include <strategy_replications/src/momentum/ts_momentum.hpp>
#include "./backtesting.hpp"
#include "./utils.hpp"

#include <string>

namespace strategy_replications {

// Example 6.1
int ts_mom(const std::string& path, const std::vector<std::string>& names, bool shorts) {
  auto df = load_csv(path);
  momentum::TsMomentum ts_mom;
  ts_mom.params_.instrument_ = utils::id_of(names.at(0));
  backtesting::Config configs;
  configs.cash_ = "1000000000";
  configs.shorts_ = shorts;
  const auto tag = ts_mom.params_.tag_; // read before the move
  const auto engine = backtesting::replay(std::move(ts_mom), df, names, configs);
  const auto& result = engine.result();
  backtesting::replay_log(tag, result);
  backtesting::summarize(df, names, result, 0);
  // Additional metrics
  return 0;
}

} // namespace strategy_replications