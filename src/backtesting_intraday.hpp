#pragma once

#include "./backtesting.hpp"

namespace strategy_replications {
namespace backtesting {

template <typename Strategy>
inline backtest::Backtester<Strategy> replay_intraday(
	Strategy strategy,
	TimeSeriesData& open, TimeSeriesData& high, TimeSeriesData& low, TimeSeriesData& close,
	const Config& configs
) {
	const auto& names = close.names_;
  // backtest::Backtester<Strategy> engine(std::move(strategy), config_for(names, configs), costs());
	backtest::Backtester<Strategy> engine(std::move(strategy), config_for(names, configs));
  // std::vector<std::size_t> columns;
  // for (const auto& name : names) {
	// 	columns.push_back(data.col(name));
	// }
  // std::vector<std::optional<Decimal64>> previous(names.size());
	// 
  for (std::size_t day = 0; day < close.dates_.size(); ++day) {
		// part == 0: strategy sees the open, submits entries (o, o, o, o)
		// part == 1: entries fill at the open; strategy submits zeros (o, h, l, c)
		// part == 2: exits fill at the close (c, c, c, c)
		for (int part = 0; part < 3; ++part) {
			for (std::size_t at = 0; at < names.size(); ++at) {
				const auto& op = open.cell_[at][day];
				const auto& cl = close.cell_[at][day];
				if (op.empty() || cl.empty()) {
					continue;
				}
				Bar bar;
				bar.timestamp_ = utils::days_from_civil(close.dates_[day]) + std::chrono::minutes(part);
				bar.open_ = Decimal64::parse(part == 2 ? cl : op);
				bar.high_ = Decimal64::parse(
					part == 0 ? op : (
						part == 2 ? cl : high.cell_[at][day]
					)
				);
				bar.low_ = Decimal64::parse(
					part == 0 ? op : (
						part == 2 ? cl : low.cell_[at][day]
					)
				);
				bar.close_ = Decimal64::parse(part == 0 ? op : cl);
				bar.volume_ = Decimal64::parse("1000000000");
				engine.feed(strategy::make_bar_event(utils::id_of(names[at]), BarInterval::ONE_MINUTE, BarStamp::OPEN, bar));
			}
		}
  }
  engine.finish();
	return engine;
}

inline void summarize_intraday(const TimeSeriesData& close, const backtest::ReplayResult& result, double cash, double capital, std::size_t t_start) {
	const std::size_t num_row = close.dates_.size();
  Eigen::ArrayXd ret = Eigen::ArrayXd::Zero(static_cast<Eigen::Index>(num_row - t_start));
  std::size_t next = 0;
  double last = cash;
  double previous = cash;
  for (std::size_t day = 0; day < num_row; ++day) {
    const auto end = utils::days_from_civil(close.dates_[day]) + std::chrono::hours(24);
    while (next < result.equity_.size() && result.equity_[next].at_ < end) {
      last = analytics::to_scalar(result.equity_[next].equity_);
      ++next;
    }
    if (day >= t_start) {
      ret[static_cast<Eigen::Index>(day - t_start)] = (last - previous) / capital;
    }
    previous = last;
  }
  const double apr = std::pow((1 + ret).prod(), 252.0 / static_cast<double>(ret.size())) - 1;
  const double ret_mean = ret.mean();
  const double sharpe = ret_mean * std::sqrt(252.0 * (ret.size() - 1.0)) / (ret - ret_mean).matrix().norm();
  std::cout << "apr=" << apr << ", sharpe=" << sharpe << std::endl;
}

} // namespace backtesting
} // namespace strategy_replications