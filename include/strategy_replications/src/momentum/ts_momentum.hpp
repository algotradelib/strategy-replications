#pragma once

#include <algotradelib/analytics>
#include <algotradelib/strategy>

#include <cmath>
#include <string>

namespace strategy_replications {

namespace analytics = algotradelib::analytics;

using algotradelib::Decimal64;
using algotradelib::InstrumentId;
using algotradelib::strategy::BarEvent;
using algotradelib::strategy::TargetPosition;

namespace momentum {

/**
 * @brief Time Series Momentum Strategy
 * 
 * Example 6.1 TU Momentum Strategy
 * 
 */
struct TsMomentum {
	struct Params {
		InstrumentId instrument_;
		std::size_t lookback_{250};
		std::size_t holddays_{25};
		double scale_{10000000};
		std::string tag_{"ts-momentum"};
	};

	Params params_;

	template <class Ctx>
	void on_bar(Ctx& ctx, const BarEvent& bar) {
		if (!(bar.instrument_ == params_.instrument_)) {
      return;
    }
		const auto* series = ctx.bars(bar.instrument_, bar.interval_);
		if (series == nullptr) {
			return;
		}
		const Eigen::VectorXd close = series->column(analytics::BarField::CLOSE);
		const auto num = static_cast<int>(close.size());
		double units = 0;
		for (int h = 0; h < params_.holddays_ - 1; ++h) {
			if (num < h + 1 + params_.lookback_) {
				break;
			}
			const double now = close[static_cast<Eigen::Index>(num - 1 - h)];
			const double then = close[static_cast<Eigen::Index>(num - 1 - h - params_.lookback_)];
			units += (now > then) - (now < then);
		}
		TargetPosition target;
		target.instrument_ = params_.instrument_;
		target.tag_ = params_.tag_;
		target.target_quantity_ = Decimal64(static_cast<std::int64_t>(
			std::llround(units * params_.scale_ / close[num - 1])),
			0
		);
		ctx.submit(target);
	}
};

} // namespace momentum
} // namespace strategy_replications