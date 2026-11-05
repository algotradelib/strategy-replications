#pragma once

#include <algotradelib/analytics>
#include <algotradelib/strategy>

#include <cmath>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace strategy_replications {

namespace analytics = algotradelib::analytics;

using algotradelib::Date;
using algotradelib::Decimal64;
using algotradelib::InstrumentId;
using algotradelib::strategy::BarSlice;
using algotradelib::strategy::TargetPosition;

namespace momentum {

/**
 * @brief Post-Earnings Announcement Drift
 *
 * `pead.py`, chapter 7: on a stock's announcement day, buy at the open when its close-to-open return is at least
 * `entry_` sample deviations of the last `lookback_` such returns (today's included, all of them defined), sell
 * short when it is at most minus that (the short wins when both hold), and leave at the close. A position is
 * `capital_ / slots_` at the open, in whole shares.
 */
struct PostEarningsDrift {
  // Keeps its own windows and never calls ctx.bars(), so its runner keeps no bar history.
  static constexpr bool reads_bar_history = false;

  struct Params {
    std::vector<InstrumentId> universe_;
    std::map<Date, std::vector<bool>> announced_; // by the session's local date, a flag per stock
    std::size_t lookback_{90};
    double entry_{0.5};
    double slots_{30};
    double capital_{1000000000};
    std::string tag_{"post-earnings-drift"};
  };

  Params params_;
  std::size_t num_asset_{0};
  std::vector<std::deque<double>> returns_; // close-to-open returns, NaN where none
  std::vector<double> close_;
  std::vector<std::int64_t> held_;

  // Which slice opens its session and which closes it, by the replay's session window.
  algotradelib::strategy::SessionEdges edges_;

  // pandas' rolling(lookback).std(): a full window of finite values, divided by count - 1.
  double deviation(std::size_t i) const {
    const auto& window = returns_[i];
    if (window.size() < params_.lookback_) {
      return analytics::missing();
    }
    double sum = 0;
    for (const double r : window) {
      if (!std::isfinite(r)) {
        return analytics::missing();
      }
      sum += r;
    }
    const double mean = sum / static_cast<double>(window.size());
    double squares = 0;
    for (const double r : window) {
      squares += (r - mean) * (r - mean);
    }
    return std::sqrt(squares / (static_cast<double>(window.size()) - 1));
  }

  template <class Ctx>
  void decide(Ctx& ctx, const BarSlice& slice) {
    const auto day = params_.announced_.find(ctx.session().local_date(slice.period_end_));
    for (std::size_t i = 0; i < num_asset_; ++i) {
      const auto* bar = slice.find(params_.universe_[i]);
      const double open = bar != nullptr ? analytics::to_scalar(*bar->bar_.open_) : analytics::missing();
      const double ret = open / close_[i] - 1;
      returns_[i].push_back(ret);
      if (returns_[i].size() > params_.lookback_) {
        returns_[i].pop_front();
      }
      if (day == params_.announced_.end() || !day->second[i]) {
        continue;
      }
      const double sd = deviation(i);
      if (!std::isfinite(sd)) {
        continue;
      }
      int side = 0;
      if (ret >= params_.entry_ * sd) {
        side = 1;
      }
      if (ret <= -params_.entry_ * sd) {
        side = -1;
      }
      if (side == 0) {
        continue;
      }
      held_[i] = side * static_cast<std::int64_t>(std::llround(params_.capital_ / params_.slots_ / open));
      submit(ctx, i, held_[i]);
    }
  }

  template <class Ctx>
  void exit_all(Ctx& ctx, const BarSlice& slice) {
    for (std::size_t i = 0; i < num_asset_; ++i) {
      const auto* bar = slice.find(params_.universe_[i]);
      close_[i] = bar != nullptr ? analytics::to_scalar(bar->bar_.close_) : analytics::missing();
      if (held_[i] != 0) {
        held_[i] = 0;
        submit(ctx, i, 0);
      }
    }
  }

  template <class Ctx>
  void submit(Ctx& ctx, std::size_t i, std::int64_t quantity) const {
    TargetPosition target;
    target.instrument_ = params_.universe_[i];
    target.tag_ = params_.tag_;
    target.target_quantity_ = Decimal64(quantity, 0);
    ctx.submit(target);
  }

  template <class Ctx>
  void on_start(Ctx&) {
    num_asset_ = params_.universe_.size();
    returns_.assign(num_asset_, {});
    close_.assign(num_asset_, analytics::missing());
    held_.assign(num_asset_, 0);
  }

  template <class Ctx>
  void on_bars(Ctx& ctx, const BarSlice& slice) {
    const auto edge = edges_.next(slice.bars_.front(), ctx.session());
    if (edge.opens_) {
      decide(ctx, slice);
    }
    if (edge.closes_) {
      exit_all(ctx, slice);
    }
  }
};

} // namespace momentum
} // namespace strategy_replications
