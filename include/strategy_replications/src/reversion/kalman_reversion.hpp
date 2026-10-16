#pragma once

#include <algotradelib/analytics>
#include <algotradelib/strategy>
#include "../statistics/kalman_filter.hpp"

#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>
#include <chrono>

namespace strategy_replications {

namespace math = algotradelib::math;
namespace analytics = algotradelib::analytics;

using algotradelib::Decimal64;
using algotradelib::Instrument;
using algotradelib::InstrumentId;
// using algotradelib::strategy::BarEvent;
using algotradelib::strategy::BarSlice;
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
	void on_bars(Ctx& ctx, const BarSlice& slice) {
		const auto* y = slice.find(params_.instrument_y_);
		const auto* x = slice.find(params_.instrument_x_);
		if (y == nullptr || x == nullptr) {
			return;
		}
		if (!kf_initialized_) {
			kf_.init(params_.delta_, params_.V_e_);
			kf_initialized_ = true;
		}
		kf_.forecast(analytics::to_scalar(y->bar_.close_), analytics::to_scalar(x->bar_.close_));
		math::Scalar resid = kf_.measurement_resid();
		math::Scalar sqrt_q = kf_.measurement_sd();
		math::Scalar hedge_ratio = kf_.hedge_ratio();
    const auto held = ctx.portfolio().net_quantity_of(params_.instrument_y_);
    if (!held) {
      return;
    }
		if (resid < -sqrt_q) {
			units_long_ = true;
		} else if (resid > -sqrt_q) {
			units_long_ = false;
		}
		if (resid > sqrt_q) {
			units_short_ = true;
		} else if (resid < sqrt_q) {
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