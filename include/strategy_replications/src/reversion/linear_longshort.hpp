#pragma once

#include <algotradelib/analytics>
#include <algotradelib/strategy>

#include <vector>

namespace strategy_replications {

namespace analytics = algotradelib::analytics;

using algotradelib::Decimal64;
using algotradelib::InstrumentId;
using algotradelib::strategy::BarSlice;
using algotradelib::strategy::TargetPosition;

namespace reversion {

/**
 * @brief Linear Long-Short Model
 * 
 * Example 4.3
 * 
 */
struct LinearLongShort {
	static constexpr bool reads_bar_history = false;

	struct Params {
		std::vector<InstrumentId> universe_;
		double scale_{1000};
		std::string tag_{"linear-long-short"};
	};

	Params params_;
	int num_asset_, count_{0};
	Eigen::ArrayXd curr_, prev_, pct_change_, weights_;
	Eigen::Array<std::int64_t, Eigen::Dynamic, 1> held_;
	bool initialized_{false};

	void init() {
		num_asset_ = params_.universe_.size();
		curr_.setZero(num_asset_);
		prev_.setZero(num_asset_);
		pct_change_.setZero(num_asset_);
		held_.setZero(num_asset_);
	}

	template <class Ctx>
	void on_bars(Ctx& ctx, const BarSlice& slice) {
		if (!initialized_) {
			init();
			initialized_ = true;
		}
		prev_ = curr_;
		for (int i = 0; i < num_asset_; ++i) {
      if (const auto* bar = slice.find(params_.universe_[i])) {
        curr_[i] = analytics::to_scalar(bar->bar_.close_);
      }
    }
		++count_;
		if (count_ == 1) {
			return;
		}
		pct_change_ = curr_ / prev_ - 1;
		const auto& is_finite = pct_change_.isFinite();
		double ret_mean = is_finite.select(pct_change_, 0.0).sum() / is_finite.count();
		weights_ = -(pct_change_ - ret_mean);
		weights_ /= weights_.isFinite().select(weights_, 0.0).abs().sum();
		// Eigen::Array<std::int64_t, Eigen::Dynamic, 1> target = (weights_ / curr_).unaryExpr([this](double x) {
		// 	return static_cast<std::int64_t>(std::llround(x * params_.scale_));
		// });
		Eigen::Array<std::int64_t, Eigen::Dynamic, 1> target = (curr_.isFinite() && curr_ > 0).select(weights_ / curr_, 0.0).unaryExpr([this](double x) {
			return static_cast<std::int64_t>(std::llround(x * params_.scale_));
		});
		// const auto& submitted = target == held_;
		for (int i = 0; i < num_asset_; ++i) {
			// if (submitted[i]) {
			// 	continue;
			// }
			if (target[i] == held_[i]) {
				continue;
			}
			held_[i] = target[i];
			TargetPosition position;
			position.instrument_ = params_.universe_[i];
			position.tag_ = params_.tag_;
			position.target_quantity_ = Decimal64(target[i], 0);
			ctx.submit(position);
		}
		// held_ = submitted.select(target, held_);
	}
};

} // namespace reversion

} // namespace strategy_replications