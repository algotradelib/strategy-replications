#pragma once

#include <algotradelib/analytics>
#include <algotradelib/strategy>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace strategy_replications {

namespace analytics = algotradelib::analytics;

using algotradelib::Decimal64;
using algotradelib::InstrumentId;
using algotradelib::strategy::BarSlice;
using algotradelib::strategy::TargetPosition;

namespace reversion {

/**
 * @brief Linear mean reversion on a fixed-weight portfolio
 *
 * Example 2.8: the portfolio is `sum(weights_[i] * close_i)`, and the strategy holds minus its z-score over
 * `lookback_` closes (sample deviation) in units; a unit is `weights_[i] * scale_` shares of instrument `i`.
 */
struct PortfolioReversion {
  // Keeps its own window and never calls ctx.bars(), so its runner keeps no bar history.
  static constexpr bool reads_bar_history = false;

  struct Params {
    std::vector<InstrumentId> instruments_;
    std::vector<double> weights_; // the first Johansen eigenvector in the book
    std::size_t lookback_{20}; // the half-life in the book
    double scale_{100000}; // shares per unit of weight
    std::string tag_{"portfolio-reversion"};
  };

  Params params_;
  std::vector<double> portfolio_;

  template <class Ctx>
  void on_bars(Ctx& ctx, const BarSlice& slice) {
    const auto count = params_.instruments_.size();
    if (params_.lookback_ < 2 || count == 0 || params_.weights_.size() != count) {
      return;
    }
    double value = 0;
    for (std::size_t i = 0; i < count; ++i) {
      const auto* bar = slice.find(params_.instruments_[i]);
      if (bar == nullptr) {
        return; // a day missing a leg is not a portfolio value
      }
      value += params_.weights_[i] * analytics::to_scalar(bar->bar_.close_);
    }
    portfolio_.push_back(value);
    if (portfolio_.size() < params_.lookback_) {
      return; // warm-up
    }
    Eigen::Map<const Eigen::VectorXd> window(
      portfolio_.data() + portfolio_.size() - params_.lookback_, params_.lookback_
    );
    const double mean = window.mean();
    const double sd = (window.array() - mean).matrix().norm() / std::sqrt(params_.lookback_ - 1.0);
    if (!(sd > 0)) {
      return;
    }
    const double units = -(window[params_.lookback_ - 1] - mean) / sd;
    for (std::size_t i = 0; i < count; ++i) {
      TargetPosition target;
      target.instrument_ = params_.instruments_[i];
      target.tag_ = params_.tag_;
      target.target_quantity_ =
        Decimal64(static_cast<std::int64_t>(std::llround(units * params_.weights_[i] * params_.scale_)), 0);
      ctx.submit(target);
    }
  }
};

} // namespace reversion
} // namespace strategy_replications
