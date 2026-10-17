#pragma once

#include <algotradelib/analytics>
#include <algotradelib/strategy>
#include "../utils/shared_window.hpp"
#include "../statistics/ols.hpp"

#include <iostream>

namespace strategy_replications {

namespace math = algotradelib::math;
namespace analytics = algotradelib::analytics;

using algotradelib::Decimal64;
using algotradelib::Instrument;
using algotradelib::InstrumentId;
using algotradelib::strategy::BarSlice;
using algotradelib::strategy::TargetPosition;

namespace reversion {

/**
 * @brief Bollinger band strategy
 * 
 * Pair trading in Example 3.1
 * 
 */
struct BollingerBand {
	struct Params {
		InstrumentId instrument_y_; // USO in the textbook
    InstrumentId instrument_x_; // GLD in the textbook
    std::size_t lookback_{20};
    math::Scalar entry_{1.0};
    math::Scalar exit_{0.0};
		std::int64_t scale_{1000}; // units * scale
    std::string tag_{"bollinger-reversion"};
  };

  Params params_;
	bool units_long_{false};
	bool units_short_{false};
	// std::vector<double> closes_y_, closes_x_;
	std::vector<double> spread_;

	template <class Ctx>
  void on_bars(Ctx& ctx, const BarSlice& slice) {
		if (params_.lookback_ < 2 || !(params_.exit_ < params_.entry_)) {
      return;
    }
		// const auto* bar_y = slice.find(params_.instrument_y_);
		// const auto* bar_x = slice.find(params_.instrument_x_);
		// if (bar_y == nullptr || bar_x == nullptr) {
		// 	return;
		// }
		if (slice.find(params_.instrument_y_) == nullptr || slice.find(params_.instrument_x_) == nullptr) {
			return;
		}
		// closes_y_.push_back(analytics::to_scalar(bar_y->bar_.close_));
		// closes_x_.push_back(analytics::to_scalar(bar_x->bar_.close_));
		// if (closes_y_.size() < params_.lookback_) {
		// 	return;
		// }
		const auto* series_y = ctx.bars(params_.instrument_y_, slice.interval_);
		const auto* series_x = ctx.bars(params_.instrument_x_, slice.interval_);
		if (series_y == nullptr || series_x == nullptr) {
      return;
    }
    // Eigen::VectorXd closes_y = series_y->column(analytics::BarField::CLOSE);
		// Eigen::VectorXd closes_x = series_x->column(analytics::BarField::CLOSE);
		// int sample_size = closes_x.size();
		// if (sample_size != closes_y.size() || sample_size < params_.lookback_) {
		// 	return;
		// }
		// Eigen::VectorXd window_y = closes_y.tail(params_.lookback_);
		// Eigen::VectorXd window_x = closes_x.tail(params_.lookback_);
		Eigen::VectorXd window_y(params_.lookback_);
		Eigen::VectorXd window_x(params_.lookback_);
		if (!utils::shared_window(*series_y, *series_x, window_y, window_x)) {
			return;
		}
		// Eigen::Map<const Eigen::VectorXd> window_y(closes_y_.data() + closes_y_.size() - params_.lookback_, params_.lookback_);
		// Eigen::Map<const Eigen::VectorXd> window_x(closes_x_.data() + closes_x_.size() - params_.lookback_, params_.lookback_);
		Eigen::Matrix<double, Eigen::Dynamic, 2> design(params_.lookback_, 2);
		design << window_x, Eigen::VectorXd::Ones(params_.lookback_);
		double hedge_ratio = statistics::fit_ols<Eigen::VectorXd, statistics::OlsAlgo::LLT>(window_y, design)[0];
		spread_.push_back(window_y[params_.lookback_ - 1] - hedge_ratio * window_x[params_.lookback_ - 1]);
		if (spread_.size() <= params_.lookback_) {
			return; // warm-up
		}
		Eigen::Map<const Eigen::VectorXd> spread(spread_.data() + spread_.size() - params_.lookback_, params_.lookback_);
		const double mean = spread.mean();
		const double sd = (spread.array() - mean).matrix().norm() / std::sqrt((params_.lookback_ - 1.0));
		const double zscore = sd > 0 ? (spread[params_.lookback_ - 1] - mean) / sd : math::undefined();
		if (!math::is_undefined(zscore)) {
			if (zscore < -params_.entry_) {
				units_long_ = true;
			} else if (zscore > -params_.exit_) {
				units_long_ = false;
			}
			if (zscore > params_.entry_) {
				units_short_ = true;
			} else if (zscore < params_.exit_) {
				units_short_ = false;
			}
		}
		std::int64_t units = units_long_ - units_short_;
		TargetPosition target_y;
		target_y.instrument_ = params_.instrument_y_;
		target_y.tag_ = params_.tag_;
		target_y.target_quantity_ = Decimal64(units * params_.scale_, 0);
		ctx.submit(target_y);
		TargetPosition target_x;
		target_x.instrument_ = params_.instrument_x_;
    target_x.tag_ = params_.tag_;
		target_x.target_quantity_ = Decimal64(
			static_cast<int64_t>(std::llround(-hedge_ratio * units * params_.scale_)),
			0
		);
    ctx.submit(target_x);
  }
};

} // namespace reversion
} // namespace strategy_replications