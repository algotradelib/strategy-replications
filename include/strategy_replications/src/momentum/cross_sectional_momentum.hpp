#pragma once

#include <algotradelib/analytics>
#include <algotradelib/strategy>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace strategy_replications {

namespace analytics = algotradelib::analytics;

using algotradelib::Decimal64;
using algotradelib::InstrumentId;
using algotradelib::strategy::BarSlice;
using algotradelib::strategy::TargetPosition;

namespace momentum {

/**
 * @brief Cross-Sectional Momentum Strategy
 *
 * Example 6.2, as `kentdaniel.py`: each close, among the stocks with a `lookback_`-day return, sorted ascending
 * (ties in universe order), the last `top_` get a long unit and the first `top_` a short unit; a stock holds the
 * sum of its units of today and the `holddays_ - 2` days before (the port's `holddays - 1` tranches). A unit is
 * `scale_` dollars at every close.
 */
struct CrossSectionalMomentum {
  // Keeps its own windows and never calls ctx.bars(), so its runner keeps no bar history.
  static constexpr bool reads_bar_history = false;

  struct Params {
    std::vector<InstrumentId> universe_;
    std::size_t lookback_{252};
    std::size_t holddays_{25};
    std::size_t top_{50};
    double scale_{100000};
    std::string tag_{"cross-sectional-momentum"};
  };

  Params params_;
  std::size_t num_asset_{0};
  std::deque<std::vector<double>> closes_; // the last lookback_ + 1 closes, NaN where no bar
  std::deque<std::vector<int>> units_; // the last holddays_ - 1 days' units
  std::vector<std::int64_t> held_;

  template <class Ctx>
  void on_start(Ctx&) {
    num_asset_ = params_.universe_.size();
    held_.assign(num_asset_, 0);
  }

  template <class Ctx>
  void on_bars(Ctx& ctx, const BarSlice& slice) {
    std::vector<double> today(num_asset_, analytics::missing());
    for (std::size_t i = 0; i < num_asset_; ++i) {
      if (const auto* bar = slice.find(params_.universe_[i])) {
        today[i] = analytics::to_scalar(bar->bar_.close_);
      }
    }
    closes_.push_back(today);
    if (closes_.size() > params_.lookback_ + 1) {
      closes_.pop_front();
    }
    std::vector<int> units(num_asset_, 0);
    if (closes_.size() == params_.lookback_ + 1) {
      const auto& then = closes_.front();
      std::vector<double> ret(num_asset_);
      std::vector<std::size_t> order;
      for (std::size_t i = 0; i < num_asset_; ++i) {
        ret[i] = today[i] / then[i] - 1;
        if (std::isfinite(ret[i])) {
          order.push_back(i);
        }
      }
      std::stable_sort(order.begin(), order.end(), [&ret](std::size_t a, std::size_t b) {
        return ret[a] < ret[b];
      });
      const auto take = std::min(params_.top_, order.size());
      // With fewer than 2 * top_ stocks one is in both, and its units cancel, as the port's do.
      for (std::size_t k = 0; k < take; ++k) {
        units[order[k]] -= 1;
      }
      for (std::size_t k = order.size() - take; k < order.size(); ++k) {
        units[order[k]] += 1;
      }
    }
    units_.push_back(std::move(units));
    if (units_.size() + 1 > params_.holddays_) {
      units_.pop_front();
    }
    for (std::size_t i = 0; i < num_asset_; ++i) {
      if (!std::isfinite(today[i])) {
        continue;
      }
      int count = 0;
      for (const auto& day : units_) {
        count += day[i];
      }
      const auto target = static_cast<std::int64_t>(std::llround(count * params_.scale_ / today[i]));
      if (target == held_[i]) {
        continue;
      }
      held_[i] = target;
      TargetPosition position;
      position.instrument_ = params_.universe_[i];
      position.tag_ = params_.tag_;
      position.target_quantity_ = Decimal64(target, 0);
      ctx.submit(position);
    }
  }
};

} // namespace momentum
} // namespace strategy_replications
