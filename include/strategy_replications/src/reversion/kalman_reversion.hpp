#pragma once

#include <algotradelib/analytics>
#include <algotradelib/strategy>
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
 */
struct KalmanReversion {
	struct Params {
		InstrumentId instrument_y_; // EWC in the textbook
    InstrumentId instrument_x_; // EWA in the textbook
		math::Scalar delta_{.0001};
		math::Scalar V_e_{.001};
		Decimal64 quantity_;
		std::string tag_{"kalman-reversion"};
	};

	Params params_;
	statistics::KalmanFilter kf_;
	bool kf_initialized_{false};

	template <class Ctx>
  void on_bar(Ctx& ctx, const BarEvent& bar) {
		if (!(bar.instrument_ == params_.instrument_y_)) {
      return; 
    }
		if (!kf_initialized_) {
			kf_.init(params_.delta_, params_.V_e_);
			kf_initialized_ = true;
		}
    const auto* series_y = ctx.bars(params_.instrument_y_, bar.interval_);
		const auto* series_x = ctx.bars(params_.instrument_x_, bar.interval_);
    if (series_y == nullptr || series_x == nullptr) {
      return;
    }
    const auto closes_y = series_y->column(analytics::BarField::CLOSE);
		const auto closes_x = series_x->column(analytics::BarField::CLOSE);
		kf_.fit(closes_y, closes_x); // Kalman filter recursions (t = 1, ..., T)
		math::Scalar price_y = closes_y[closes_y.size() - 1];
    math::Scalar price_x = closes_x[closes_x.size() - 1];
		kf_.forecast(price_y, price_x); // Kalman filter forecasting (t > T)
		math::Scalar resid = kf_.measurement_resid();
		math::Scalar sqrt_q = kf_.measurement_sd();
    const auto held = ctx.portfolio().net_quantity_of(params_.instrument_y_);
    if (!held) {
      return;
    }
    const bool holding = held->coefficient() != 0;
		TargetPosition target;
    target.instrument_ = params_.instrument_y_;
    target.tag_ = params_.tag_;
		if (!holding && resid < -sqrt_q) {
			target.target_quantity_ = params_.quantity_;
		} else if (holding && resid > -sqrt_q) {
			target.target_quantity_ = Decimal64::parse("0");
		} else {
			return;
		}
		// shorts: resid vs sqrt_q
		// algotradelib does not support shorts yet
    ctx.submit(target);
  }
};

} // namespace reversion
} // namespace strategy_replications