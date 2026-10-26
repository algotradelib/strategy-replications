#pragma once

#include <algotradelib/analytics>
#include <algotradelib/strategy>

#include <string>
#include <cmath>
#include <vector>
#include <chrono>
#include <numeric>
#include <algorithm>

namespace strategy_replications {

namespace analytics = algotradelib::analytics;

using algotradelib::Decimal64;
using algotradelib::InstrumentId;
using algotradelib::strategy::BarSlice;
using algotradelib::strategy::TargetPosition;

namespace reversion {

/**
 * @brief Buy-on-Gap Model
 * 
 * Intraday mean reversion, Example 4.1
 * 
 */
struct BuyOnGap {
	// Keeps its own windows and never calls ctx.bars(), so its runner keeps no bar history.
	static constexpr bool reads_bar_history = false;

	struct Params {
		std::vector<InstrumentId> universe_;
		int top_{10};
		int lookback_{20};
		int window_{90};
		double entry_{1.0};
		double capital_{1000000000};
		// double scale_{1000};
		std::string tag_{"buy-on-gap"};
	};

	Params params_;
	int num_asset_, buffer_size_, bar_count_{0};
	Eigen::ArrayXXd close_buffer_;
	Eigen::ArrayXd pct_change_, pct_change_remove_;
	Eigen::ArrayXd ret_sum_, ret_ss_, ret_sd_, close_sum_;
	Eigen::ArrayXd prev_close_, curr_close_, curr_open_, prev_low_, gap_, positions_;
	Eigen::Array<bool, Eigen::Dynamic, 1> gapped_;
	Eigen::VectorXi ret_id_;
	bool initialized_{false};
	std::vector<bool> held_;

	// 0: (o, o, o, o), 1: (o, h, l, c), 2: b3 (c, c, c, c)
	static int part_of(std::chrono::system_clock::time_point end) {
		const auto midnight = std::chrono::floor<std::chrono::hours>(end);
		return static_cast<int>(std::chrono::duration_cast<std::chrono::minutes>(end - midnight).count()) - 1;
	}

	void init() {
		num_asset_ = params_.universe_.size();
		buffer_size_ = std::max(params_.window_ + 2, params_.lookback_ + 1);
		close_buffer_ = Eigen::ArrayXXd::Zero(buffer_size_, num_asset_);
		// ret_buffer_ = Eigen::ArrayXXd::Zero(params_.window_, num_asset_);
		// close_buffer_ = Eigen::ArrayXXd::Zero(params_.lookback_, num_asset_);
		// ret_buffer_ = Eigen::ArrayXXd::Zero(params_.window_, num_asset_);
		// close_lookback_ = Eigen::ArrayXXd::Zero(params_.lookback_, num_asset_);
		pct_change_ = Eigen::ArrayXd::Zero(num_asset_);
		pct_change_remove_ = Eigen::ArrayXd::Zero(num_asset_);
		ret_sum_ = Eigen::ArrayXd::Zero(num_asset_);
		ret_ss_ = Eigen::ArrayXd::Zero(num_asset_);
		ret_sd_ = Eigen::ArrayXd::Zero(num_asset_);
		close_sum_ = Eigen::ArrayXd::Zero(num_asset_);
		// back_close_ = Eigen::ArrayXd::Zero(num_asset_);
		prev_close_ = Eigen::ArrayXd::Zero(num_asset_);
		curr_close_ = Eigen::ArrayXd::Zero(num_asset_);
		curr_open_ = Eigen::ArrayXd::Zero(num_asset_);
		prev_low_ = Eigen::ArrayXd::Zero(num_asset_);
		// curr_low_ = Eigen::ArrayXd::Zero(num_asset_);
		gap_ = Eigen::ArrayXd::Zero(num_asset_);
		positions_ = Eigen::ArrayXd::Zero(num_asset_);
		gapped_ = Eigen::Array<bool, Eigen::Dynamic, 1>::Constant(num_asset_, false);
		ret_id_ = Eigen::VectorXi::Zero(num_asset_);
		held_.assign(num_asset_, false);
	}

	template <class Ctx>
	void decide(Ctx& ctx, const BarSlice& slice) {
		curr_open_.setConstant(analytics::missing());
    for (int i = 0; i < num_asset_; ++i) {
      if (const auto* bar = slice.find(params_.universe_[i])) {
        curr_open_[i] = analytics::to_scalar(*bar->bar_.open_);
      }
    }
		if (bar_count_ < params_.window_ + 1 || bar_count_ < params_.lookback_) {
      return;
    }
    ret_sd_ = ((ret_ss_ - ret_sum_.square() / params_.window_) / (params_.window_ - 1)).sqrt();
    gap_ = curr_open_ / prev_low_ - 1;
    gapped_ = gap_.isFinite() * (curr_open_ < prev_low_ * (1 - params_.entry_ * ret_sd_)) * (curr_open_ > close_sum_ / params_.lookback_);
    const int num_gapped = gapped_.count();
		positions_.setZero();
		if (num_gapped > params_.top_) {
			std::iota(std::begin(ret_id_), std::begin(ret_id_) + num_asset_, 0);
			std::nth_element(std::begin(ret_id_), std::begin(ret_id_) + params_.top_, std::end(ret_id_),
				[this](int i1, int i2) {
					const bool gapped1 = gapped_(i1);
					const bool gapped2 = gapped_(i2);
					if (gapped1 != gapped2) {
						return gapped1;
					}
					return gapped1 && gap_(i1) < gap_(i2); // ascending among gap-downed columns
				}
			);
			positions_(ret_id_(Eigen::seqN(0, params_.top_))) = 1.0;
		} else if (num_gapped > 0) {
			positions_ += gapped_.cast<double>();
		}
    for (int i = 0; i < num_asset_; ++i) {
      if (positions_[i] <= 0) {
        continue;
      }
      TargetPosition target;
      target.instrument_ = params_.universe_[i];
      target.tag_ = params_.tag_;
      target.target_quantity_ = Decimal64(
				static_cast<std::int64_t>(std::floor(params_.capital_ / params_.top_ / curr_open_[i])),
				0
			);
      ctx.submit(target);
      held_[i] = true;
    }
	}

	template <class Ctx>
	void exit_all(Ctx& ctx, const BarSlice& slice) {
		for (int i = 0; i < num_asset_; ++i) {
      if (!held_[i]) {
        continue;
      }
      TargetPosition target;
      target.instrument_ = params_.universe_[i];
      target.tag_ = params_.tag_;
      target.target_quantity_ = Decimal64::parse("0");
      ctx.submit(target);
      held_[i] = false;
    }
    prev_low_.setConstant(analytics::missing());
    for (int i = 0; i < num_asset_; ++i) {
      if (const auto* bar = slice.find(params_.universe_[i])) {
        prev_low_[i] = analytics::to_scalar(bar->bar_.low_);
      }
    }
	}

	void push_close(const BarSlice& slice) {
		curr_close_.setConstant(analytics::missing());
    for (int i = 0; i < num_asset_; ++i) {
      if (const auto* bar = slice.find(params_.universe_[i])) {
        curr_close_[i] = analytics::to_scalar(bar->bar_.close_);
      }
    }
		close_buffer_.row(bar_count_ % buffer_size_) = curr_close_;
    ++bar_count_;
    close_sum_ += curr_close_;
		if (bar_count_ > params_.lookback_) {
			close_sum_ -= close_buffer_.row((bar_count_ - 1 - params_.lookback_) % buffer_size_);
		}
    if (bar_count_ >= 2) {
      pct_change_ = curr_close_ / prev_close_ - 1;
      ret_sum_ += pct_change_;
      ret_ss_ += pct_change_.square();
    }
    if (bar_count_ >= params_.window_ + 2) {
			pct_change_remove_ = close_buffer_.row((bar_count_ - 1 - params_.window_) % buffer_size_) /
				close_buffer_.row((bar_count_ - 2 - params_.window_) % buffer_size_) - 1;
      ret_sum_ -= pct_change_remove_;
      ret_ss_  -= pct_change_remove_.square();
    }
    prev_close_ = curr_close_;
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
		// 
		// prev_close_ = curr_close_;
		// prev_low_ = curr_low_;
		// for (int i = 0; i < num_asset_; ++i) {
		// 	if (const auto* bar = slice.find(params_.universe_[i])) {
		// 		curr_close_[i] = analytics::to_scalar(bar->bar_.close_);
		// 		curr_open_[i] = bar->bar_.open_ ? analytics::to_scalar(*bar->bar_.open_) : analytics::missing();
		// 		curr_low_[i] = analytics::to_scalar(bar->bar_.low_);
		// 	}
		// }
		// close_buffer_.row(bar_count_ % buffer_size_) = curr_close_;
		// ++bar_count_;
		// if (bar_count_ == 1) {
		// 	return;
		// }
		// pct_change_ = curr_close_ / prev_close_ - 1;
		// ret_sum_ += pct_change_;
		// ret_ss_ += pct_change_.square();
		// close_sum_ += curr_close_;
		// if (bar_count_ > params_.lookback_) {
		// 	close_sum_ -= close_buffer_.row((bar_count_ - params_.lookback_) % buffer_size_);
		// }
		// if (bar_count_ < params_.window_ + 1) {
		// 	return;
		// }
		// pct_change_remove_ = close_buffer_.row((bar_count_ - 1 - params_.window_) % buffer_size_) /
		// 	close_buffer_.row((bar_count_ - 2 - params_.window_) % buffer_size_) - 1;
		// ret_sum_ -= pct_change_remove_;
		// ret_ss_ -= pct_change_remove_.square();
		// ret_sd_ = ((ret_ss_ - ret_sum_.square() / params_.window_) / (params_.window_ - 1)).sqrt();
		// gap_ = curr_open_ / prev_low_ - 1;
		// gapped_ = gap_.isFinite() * (curr_open_ < prev_low_ * (1 - params_.entry_ * ret_sd_)) * (curr_open_ > close_sum_ / params_.lookback_);
		// const int num_gapped = gapped_.count();
		// positions_.setZero();
		// if (num_gapped > params_.top_) {
		// 	std::iota(std::begin(ret_id_), std::begin(ret_id_) + num_asset_, 0);
		// 	std::nth_element(std::begin(ret_id_), std::begin(ret_id_) + params_.top_, std::end(ret_id_),
		// 		[this](int i1, int i2) {
		// 			const bool gapped1 = gapped_(i1);
		// 			const bool gapped2 = gapped_(i2);
		// 			if (gapped1 != gapped2) {
		// 				return gapped1;
		// 			}
		// 			return gapped1 && gap_(i1) < gap_(i2); // ascending among gap-downed columns
		// 		}
		// 	);
		// 	positions_(ret_id_(Eigen::seqN(0, params_.top_))) = 1.0;
		// } else if (num_gapped > 0) {
		// 	positions_ += gapped_.cast<double>();
		// }
		// for (int i = 0; i < std::min(num_gapped, params_.top_); ++i) {
		// 	if (positions_[i] > 0) {
		// 		TargetPosition target;
		// 		target.instrument_ = params_.universe_[i];
		// 		target.tag_ = params_.tag_;
		// 		target.target_quantity_ = Decimal64(
		// 			static_cast<std::int64_t>(std::floor(params_.capital_ / static_cast<double>(params_.top_) / curr_open_[i] + 0.5)),
		// 			0
		// 		);
		// 		ctx.submit(target);
		// 	}
		// }
	}
};

} // namespace reversion
} // namespace strategy_replications