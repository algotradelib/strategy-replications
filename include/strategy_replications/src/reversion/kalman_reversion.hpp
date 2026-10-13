#pragma once

#include <algotradelib/analytics>
#include <algotradelib/strategy>
#include <cmath>
#include <cstdint>
#include "../statistics/kalman_filter.hpp"
#include "algotradelib/src/core/types.hpp"
#include "algotradelib/src/math/scalar.hpp"
#include "algotradelib/src/strategy/intent.hpp"

namespace strategy_replications {

namespace math = algotradelib::math;
namespace analytics = algotradelib::analytics;

using algotradelib::Decimal64;
using algotradelib::Instrument;
using algotradelib::InstrumentId;
using algotradelib::strategy::BarEvent;
using algotradelib::strategy::TargetPosition;

namespace reversion {

/**
 * @brief Kalman Filter Mean Reversion Strategy
 *
 * Pair trading in Example 3.3
 * 
 */
struct KalmanReversion {
	struct Params {
		InstrumentId instrument_y_; // EWC in the textbook
    InstrumentId instrument_x_; // EWA in the textbook
		math::Scalar delta_{.0001};
		math::Scalar V_e_{.001};
		Decimal64 quantity_;
		std::int64_t scale_{1000}; // units * scale
		std::string tag_{"kalman-reversion"};
	};

	Params params_;
	bool units_long_{false};
	bool units_short_{false};
	statistics::KalmanFilter kf_;
	bool kf_initialized_{false};

	template <class Ctx>
  void on_bar(Ctx& ctx, const BarEvent& bar) {
		if (!(bar.instrument_ == params_.instrument_y_)) {
      return; 
    }
    const auto* series_y = ctx.bars(params_.instrument_y_, bar.interval_);
		const auto* series_x = ctx.bars(params_.instrument_x_, bar.interval_);
    if (series_y == nullptr || series_x == nullptr) {
      return;
    }
    const Eigen::VectorXd closes_y = series_y->column(analytics::BarField::CLOSE);
		const Eigen::VectorXd closes_x = series_x->column(analytics::BarField::CLOSE);
		if (!kf_initialized_) {
			kf_.init(params_.delta_, params_.V_e_);
			kf_.fit(closes_y.head(closes_y.size() - 1), closes_x.head(closes_y.size() - 1)); // Kalman filter recursions (t = 1, ..., T)
			kf_initialized_ = true;
		}
		math::Scalar price_y = closes_y[closes_y.size() - 1];
    math::Scalar price_x = closes_x[closes_x.size() - 1];
		kf_.forecast(price_y, price_x); // Kalman filter forecasting (t > T)
		math::Scalar resid = kf_.measurement_resid();
		math::Scalar sqrt_q = kf_.measurement_sd();
		math::Scalar hedge_ratio = kf_.hedge_ratio();
    const auto held = ctx.portfolio().net_quantity_of(params_.instrument_y_);
    if (!held) {
      return;
    }
		// const bool holding = held->coefficient() != 0;
		if (resid < -sqrt_q) {
			units_long_ = true;
		}
		if (resid > 0) {
			units_long_ = false;
		}
		if (resid > sqrt_q) {
			units_short_ = true;
		}
		if (resid < 0) {
			units_short_ = false;
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