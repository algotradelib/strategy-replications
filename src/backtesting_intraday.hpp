#pragma once

#include "./backtesting.hpp"

namespace strategy_replications {
namespace backtesting {

template <typename Strategy>
inline backtest::Backtester<Strategy> replay_intraday(
  Strategy strategy, TimeSeriesData& open, TimeSeriesData& high, TimeSeriesData& low, TimeSeriesData& close,
  const Config& configs
) {
  const auto& names = close.names_;
  const auto config = config_for(names, configs);
  const auto session = config.session_;
  backtest::Backtester<Strategy> engine(
    std::move(strategy), config, costs_none().with_market_price("decision-close", decision_close)
  );
  for (std::size_t day = 0; day < close.dates_.size(); ++day) {
    // Each day as two daily bars stamped at the session's open and close: the open alone, a point (the strategy's
    // session opens there, and it submits entries), then the day (its session closes: entries fill at the open they
    // were decided on, and it submits exits, which fill at this close on the next day's point).
    const auto noon = utils::days_from_civil(close.dates_[day]) + std::chrono::hours(12) - session.utc_offset_;
    for (int part = 0; part < 2; ++part) {
      for (std::size_t at = 0; at < names.size(); ++at) {
        const auto& op = open.cell_[at][day];
        const auto& cl = close.cell_[at][day];
        if (op.empty() || cl.empty()) {
          continue;
        }
        Bar bar;
        bar.timestamp_ = part == 0 ? session.opens_at(noon) : session.closes_at(noon);
        bar.open_ = Decimal64::parse(op);
        bar.high_ = Decimal64::parse(part == 0 ? op : high.cell_[at][day]);
        bar.low_ = Decimal64::parse(part == 0 ? op : low.cell_[at][day]);
        bar.close_ = Decimal64::parse(part == 0 ? op : cl);
        bar.volume_ = Decimal64::parse("1000000000");
        engine.feed(strategy::make_bar_event(utils::id_of(names[at]), BarInterval::ONE_DAY, BarStamp::CLOSE, bar));
      }
    }
  }
  engine.finish();
  return engine;
}

inline void summarize_intraday(
  const TimeSeriesData& close, const backtest::ReplayResult& result, double capital, std::size_t t_start
) {
  const std::size_t num_row = close.dates_.size();
  Eigen::ArrayXd ret = Eigen::ArrayXd::Zero(num_row - t_start);
  std::size_t next = 0;
  double last = capital;
  double previous = capital;
  for (std::size_t day = 0; day < num_row; ++day) {
    const auto end = utils::days_from_civil(close.dates_[day]) + std::chrono::hours(24);
    while (next < result.equity_.size() && result.equity_[next].at_ <= end) {
      last = analytics::to_scalar(result.equity_[next].equity_);
      ++next;
    }
    if (day >= t_start) {
      ret[static_cast<int>(day - t_start)] = (last - previous) / capital;
    }
    previous = last;
  }
  const double apr = std::pow((1 + ret).prod(), 252.0 / ret.size()) - 1;
  const double ret_mean = ret.mean();
  const double sharpe = ret_mean * std::sqrt(252.0 * (ret.size() - 1.0)) / (ret - ret_mean).matrix().norm();
  std::cout << "apr=" << apr << ", sharpe=" << sharpe << std::endl;
}

} // namespace backtesting
} // namespace strategy_replications