#pragma once

#include <algotradelib/analytics>
#include <algotradelib/strategy>

#include <cmath>
#include <cstdint>
#include <deque>
#include <string>

namespace strategy_replications {

namespace analytics = algotradelib::analytics;

using algotradelib::Decimal64;
using algotradelib::InstrumentId;
using algotradelib::strategy::BarSlice;
using algotradelib::strategy::TargetPosition;

namespace momentum {

/**
 * @brief Opening Gap Strategy
 *
 * Example 7.1, as `gapFutures_FSTX.m`: at the open, sell when it is at or above yesterday's high by `entry_`
 * deviations of the last `window_` close-to-close returns, buy when it is at or below yesterday's low by as much
 * (the buy wins when both hold), and leave at the close. A position is `capital_` at the open, in whole units.
 */
struct OpeningGap {
  // Keeps its own windows and never calls ctx.bars(), so its runner keeps no bar history.
  static constexpr bool reads_bar_history = false;

  struct Params {
    InstrumentId instrument_;
    double entry_{0.1};
    std::size_t window_{90};
    double capital_{1000000000};
    std::string tag_{"opening-gap"};
  };

  Params params_;
  std::deque<double> returns_; // the last window_ returns up to yesterday, NaN for the first
  double high_{analytics::missing()};
  double low_{analytics::missing()};
  double close_{analytics::missing()};
  std::int64_t held_{0};

  // Which slice opens its session and which closes it, by the replay's session window.
  algotradelib::strategy::SessionEdges edges_;

  // MATLAB's smartstd: the population deviation of the finite values.
  double deviation() const {
    double sum = 0;
    std::size_t counted = 0;
    for (const double r : returns_) {
      if (std::isfinite(r)) {
        sum += r;
        ++counted;
      }
    }
    if (counted == 0) {
      return analytics::missing();
    }
    const double mean = sum / static_cast<double>(counted);
    double squares = 0;
    for (const double r : returns_) {
      if (std::isfinite(r)) {
        squares += (r - mean) * (r - mean);
      }
    }
    return std::sqrt(squares / static_cast<double>(counted));
  }

  template <class Ctx>
  void decide(Ctx& ctx, const BarSlice& slice) {
    const auto* bar = slice.find(params_.instrument_);
    if (bar == nullptr || returns_.size() < params_.window_) {
      return;
    }
    const double sd = deviation();
    const double open = analytics::to_scalar(*bar->bar_.open_);
    const auto units = static_cast<std::int64_t>(std::llround(params_.capital_ / open));
    if (open >= high_ * (1 + params_.entry_ * sd)) {
      held_ = -units;
    }
    if (open <= low_ * (1 - params_.entry_ * sd)) {
      held_ = units;
    }
    if (held_ != 0) {
      submit(ctx, held_);
    }
  }

  template <class Ctx>
  void exit_all(Ctx& ctx) {
    if (held_ != 0) {
      held_ = 0;
      submit(ctx, 0);
    }
  }

  void push_close(const BarSlice& slice) {
    const auto* bar = slice.find(params_.instrument_);
    if (bar == nullptr) {
      return;
    }
    const double close = analytics::to_scalar(bar->bar_.close_);
    returns_.push_back(close / close_ - 1);
    if (returns_.size() > params_.window_) {
      returns_.pop_front();
    }
    high_ = analytics::to_scalar(bar->bar_.high_);
    low_ = analytics::to_scalar(bar->bar_.low_);
    close_ = close;
  }

  template <class Ctx>
  void submit(Ctx& ctx, std::int64_t quantity) const {
    TargetPosition target;
    target.instrument_ = params_.instrument_;
    target.tag_ = params_.tag_;
    target.target_quantity_ = Decimal64(quantity, 0);
    ctx.submit(target);
  }

  template <class Ctx>
  void on_bars(Ctx& ctx, const BarSlice& slice) {
    const auto edge = edges_.next(slice.bars_.front(), ctx.session());
    if (edge.opens_) {
      decide(ctx, slice);
    }
    if (edge.closes_) {
      exit_all(ctx);
      push_close(slice);
    }
  }
};

} // namespace momentum
} // namespace strategy_replications
