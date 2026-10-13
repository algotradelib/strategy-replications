#pragma once

#include <algotradelib/backtest>
#include <algotradelib/strategy>
#include "./data.hpp"
#include "./utils.hpp"
#include "algotradelib/src/backtest/engine.hpp"

namespace strategy_replications {

namespace backtest = algotradelib::backtest;
namespace strategy = algotradelib::strategy;

namespace backtesting {

struct Config {
	std::string cash_{"100000"};
	bool shorts_{true};
	std::string commission{"0"};
	// int trace_{0};
};

struct Held {
  std::vector<std::vector<double>> shares; // [name][day]
  std::size_t executions{0};
  std::size_t refused{0};
  std::size_t rejected{0};
  std::size_t unfilled{0};
  std::string halted;
  double final_equity{0};
};

inline backtest::ReplayConfig config_for(const std::vector<std::string>& names, const Config& configs) {
	backtest::ReplayConfig replay;
	replay.cash_ = atl::Decimal64::parse(configs.cash_);
	for (const auto& name : names) {
		replay.instruments_.push_back(utils::reference(name));
	}
  replay.series_capacity_ = 512;
  replay.limits_.permitted_phases_ = std::nullopt;
  if (configs.shorts_) {
    replay.limits_.enforce_sellable_quantity_ = false;
  }
  return replay;
}

backtest::NextBarFillModel costs() {
  return backtest::NextBarFillModel(
    backtest::CommissionModel::of(Decimal64::parse("0.00015"), Decimal64::parse("100")),
    backtest::SlippageModel::of_ticks(0)
  );
}

template <typename Strategy>
backtest::Backtester<Strategy> replay(Strategy strategy, TimeSeriesData& data, const std::vector<std::string>& names, const Config& configs) {
  atl::backtest::Backtester<Strategy> engine(std::move(strategy), config_for(names, configs), costs());
  std::vector<std::size_t> columns;
  for (const auto& name : names) {
		columns.push_back(data.col(name));
	}
  for (std::size_t day = 0; day < data.dates_.size(); ++day) {
    for (std::size_t at = 0; at < names.size(); ++at) {
      const auto& text = data.cell_[columns[at]][day];
      if (text.empty()) {
				continue;
			}
      engine.feed(strategy::make_bar_event(utils::id_of(names[at]), BarInterval::ONE_DAY, BarStamp::CLOSE, utils::bar_of(data.dates_[day], text)));
    }
  }
  engine.finish();
  return engine;
}

} // namespace backtesting
} // namespace strategy_replications