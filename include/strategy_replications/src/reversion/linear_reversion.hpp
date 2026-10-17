#pragma once

#include <algotradelib/analytics>
#include <algotradelib/strategy>
#include "../utils/shared_window.hpp"
#include "../statistics/ols.hpp"

#include <cmath>
#include <vector>

namespace strategy_replications {

namespace math = algotradelib::math;
namespace analytics = algotradelib::analytics;

using algotradelib::Decimal64;
using algotradelib::InstrumentId;
using algotradelib::strategy::BarSlice;
using algotradelib::strategy::TargetPosition;

namespace reversion {

// pp64
enum class LinearSpread {
	PRICE, // price spreads
	LOG, // log price spreads
	RATIO, // ratio
};

/**
 * @brief Linear Reversion Strategy
 * 
 * Pair trading in example 3.1
 * 
 */
template <LinearSpread Spread = LinearSpread::PRICE>
struct LinearReversion {
	struct Params {
		InstrumentId instrument_y_; // USO in the textbook
		InstrumentId instrument_x_; // GLD in the textbook
		// LinearSpread spread_{LinearSpread::PRICE};
		std::size_t lookback_{20};
		double scale_{1000}; // shares per unit (PRICE), or dollars per unit (LOG, RATIO)
		std::string tag_{"linear-reversion"};
	};

	Params params_;
	std::vector<double> spread_;

	template <class Ctx>
  void on_bars(Ctx& ctx, const BarSlice& slice) {
		if (params_.lookback_ < 2) {
      return;
    }
		if (slice.find(params_.instrument_y_) == nullptr || slice.find(params_.instrument_x_) == nullptr) {
			return;
		}
		const auto* series_y = ctx.bars(params_.instrument_y_, slice.interval_);
		const auto* series_x = ctx.bars(params_.instrument_x_, slice.interval_);
		if (series_y == nullptr || series_x == nullptr) {
      return;
    }
		// The ratio needs only today's closes; the two spreads need a regression window first.
		const std::size_t needed = (Spread == LinearSpread::RATIO) ? 1 : params_.lookback_;
		Eigen::VectorXd window_y(needed);
		Eigen::VectorXd window_x(needed);
		if (!utils::shared_window(*series_y, *series_x, window_y, window_x)) {
			return;
		}
		const double y = window_y[needed - 1];
		const double x = window_x[needed - 1];
		double hedge_ratio = 1.0;
		if constexpr (Spread == LinearSpread::RATIO) {
			spread_.push_back(y / x);
		} else {
			if constexpr (Spread == LinearSpread::LOG) {
				window_y = window_y.array().log();
				window_x = window_x.array().log();
			}
			// Make this hedge ratio computation as inline function (also in bollinger band)
			Eigen::Matrix<double, Eigen::Dynamic, 2> design(params_.lookback_, 2);
			design << window_x, Eigen::VectorXd::Ones(params_.lookback_);
			hedge_ratio = statistics::fit_ols<Eigen::VectorXd, statistics::OlsAlgo::LLT>(window_y, design)[0];
			spread_.push_back(window_y[params_.lookback_ - 1] - hedge_ratio * window_x[params_.lookback_ - 1]);
		}
		if (spread_.size() < params_.lookback_) {
			return; // warm-up
		}
		Eigen::Map<const Eigen::VectorXd> spread(spread_.data() + spread_.size() - params_.lookback_, params_.lookback_);
		const double mean = spread.mean();
		const double sd = (spread.array() - mean).matrix().norm() / std::sqrt(params_.lookback_ - 1.0);
		if (!(sd > 0)) {
			return;
		}
		const double units = -(spread[params_.lookback_ - 1] - mean) / sd;
		TargetPosition target_y, target_x;
		target_y.instrument_ = params_.instrument_y_;
		target_y.tag_ = params_.tag_;
		target_x.instrument_ = params_.instrument_x_;
    target_x.tag_ = params_.tag_;
		double shares_y, shares_x;
		if constexpr (Spread == LinearSpread::PRICE) {
			shares_y = units * params_.scale_;
			shares_x = -hedge_ratio * units * params_.scale_;
		} else if constexpr (Spread == LinearSpread::LOG) {
			shares_y = units * params_.scale_ / y;
			shares_x = -hedge_ratio * units * params_.scale_ / x;
		} else if constexpr (Spread == LinearSpread::RATIO) {
			shares_y = units * params_.scale_ / y;
			shares_x = -units * params_.scale_ / x;
		}
		target_y.target_quantity_ = Decimal64(static_cast<std::int64_t>(std::llround(shares_y)), 0);
		ctx.submit(target_y);
		target_x.target_quantity_ = Decimal64(static_cast<std::int64_t>(std::llround(shares_x)), 0);
		ctx.submit(target_x);
  }
};

} // namespace reversion
} // namespace strategy_replications