#pragma once

#include <strategy_replications/src/momentum/cross_sectional_momentum.hpp>
#include "./backtesting.hpp"
#include "./utils.hpp"

#include <algorithm>
#include <iostream>
#include <string>

namespace strategy_replications {

// Example 6.2
int xs_mom(const std::string& directory, bool shorts) {
  auto df = load_ohlc(directory, "cl");
  momentum::CrossSectionalMomentum xs_mom;
  for (const auto& name : df.names_) {
    xs_mom.params_.universe_.push_back(utils::id_of(name));
  }
  backtesting::Config configs;
  configs.cash_ = "1000000000";
  configs.shorts_ = shorts;
  const auto& params = xs_mom.params_;
  const double gross = 2.0 * params.top_ * params.holddays_ * params.scale_; // read before the move
  const auto tag = params.tag_; // read before the move
  const auto engine = backtesting::replay(std::move(xs_mom), df, df.names_, configs);
  const auto& result = engine.result();
  backtesting::replay_log(tag, result);
  backtesting::summarize(df, df.names_, result, 0);
  // The book's figure: the same replay from 2007-05-15 to 2007-12-31, each day's P&L over the fixed gross of the
  // port, 2 * top * holddays units.
  auto window = df;
  window.slice_dates(df.dates_.front(), 20071231);
  const auto start = std::lower_bound(window.dates_.begin(), window.dates_.end(), 20070515) - window.dates_.begin();
  std::cout << "2007-05-15 to 2007-12-31 over a fixed gross of " << gross << ": ";
  backtesting::summarize(window, df.names_, result, start, gross);
  return 0;
}

} // namespace strategy_replications
