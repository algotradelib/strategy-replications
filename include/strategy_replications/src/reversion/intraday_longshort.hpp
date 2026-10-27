#pragma once

#include <algotradelib/analytics>
#include <algotradelib/strategy>

#include <chrono>
#include <vector>

namespace strategy_replications {

namespace analytics = algotradelib::analytics;

using algotradelib::Decimal64;
using algotradelib::InstrumentId;
using algotradelib::strategy::BarSlice;
using algotradelib::strategy::TargetPosition;

namespace reversion {

/**
 * @brief Intraday Long-Short Model
 * 
 * Example 4.4
 * 
 */
struct IntradayLongShort {
	static constexpr bool reads_bar_history = false;

	struct Params {
		std::vector<InstrumentId> universe_;
		double capital_{1000000000};
		std::string tag_{"intraday-long-short"};
	};

	Params params_;
	int num_asset_, count_{0};
	Eigen::ArrayXd open_, close_, pct_change_, weights_;
	Eigen::Array<std::int64_t, Eigen::Dynamic, 1> held_;
	bool initialized_{false};

	static int part_of(std::chrono::system_clock::time_point end) {
		const auto midnight = std::chrono::floor<std::chrono::hours>(end);
		return static_cast<int>(std::chrono::duration_cast<std::chrono::minutes>(end - midnight).count()) - 1;
	}

	void init() {
		num_asset_ = params_.universe_.size();
		open_.setZero(num_asset_);
		close_.setZero(num_asset_);
		pct_change_.setZero(num_asset_);
		weights_.setZero(num_asset_);
		held_.setZero(num_asset_);
	}

	template <class Ctx>
	void decide(Ctx& ctx, const BarSlice& slice) {
		for (int i = 0; i < num_asset_; ++i) {
      if (const auto* bar = slice.find(params_.universe_[i])) {
				open_[i] = analytics::to_scalar(*bar->bar_.open_);
      }
    }
		++count_;
		if (count_ == 1) {
			return;
		}
		pct_change_ = open_ / close_ - 1;
		const auto& is_finite = pct_change_.isFinite();
		double ret_mean = is_finite.select(pct_change_, 0.0).sum() / is_finite.count();
		weights_ = -(pct_change_ - ret_mean);
		weights_ /= weights_.isFinite().select(weights_, 0.0).abs().sum();
		Eigen::Array<std::int64_t, Eigen::Dynamic, 1> target = (open_.isFinite() && open_ > 0).select(weights_ / open_, 0.0).unaryExpr([this](double x) {
			return static_cast<std::int64_t>(std::llround(x * params_.capital_));
		});
		for (int i = 0; i < num_asset_; ++i) {
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
	}

	template <class Ctx>
	void exit_all(Ctx& ctx, const BarSlice& slice) {
		for (int i = 0; i < num_asset_; ++i) {
			if (held_[i] == 0) {
				continue;
			}
			TargetPosition position;
			position.instrument_ = params_.universe_[i];
			position.tag_ = params_.tag_;
			position.target_quantity_ = Decimal64::parse("0");
			ctx.submit(position);
			held_[i] = 0;
		}
	}

	void push_close(const BarSlice& slice) {
		for (int i = 0; i < num_asset_; ++i) {
      if (const auto* bar = slice.find(params_.universe_[i])) {
        close_[i] = analytics::to_scalar(bar->bar_.close_);
      }
    }
	}

	template <class Ctx>
	void on_bars(Ctx& ctx, const BarSlice& slice) {
		if (!initialized_) {
			init();
			initialized_ = true;
		}
		switch (part_of(slice.period_end_)) {
			case 0:
				decide(ctx, slice);
				return;
			case 1:
				exit_all(ctx, slice);
				return;
			case 2:
				push_close(slice);
				return;
			default:
				return;
		}
	}
};

} // namespace reversion

} // namespace strategy_replications