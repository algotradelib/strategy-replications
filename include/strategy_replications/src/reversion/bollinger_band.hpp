#pragma once

#include <algotradelib/analytics>
#include <algotradelib/strategy>
#include "../statistics/ols.hpp"

#include <iostream>

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
	std::optional<std::chrono::system_clock::time_point> stepped_y_, stepped_x_;
	// statistics::Ols<Eigen::VectorXd> ols_;
	// bool ols_initialized_{false};
	std::vector<double> spread_;

	template <class Ctx>
  void on_bar(Ctx& ctx, const BarEvent& bar) {
		if (params_.lookback_ < 2 || !(params_.exit_ < params_.entry_)) {
      return;
    }
		if (bar.instrument_ == params_.instrument_y_) {
      stepped_y_ = bar.bar_.timestamp_;
    } else if (bar.instrument_ == params_.instrument_x_) {
			stepped_x_ = bar.bar_.timestamp_;
		} else {
			return;
		}
		if (stepped_y_ != stepped_x_) {
			return; // pair trading
		}
    const auto* series_y = ctx.bars(params_.instrument_y_, bar.interval_);
		const auto* series_x = ctx.bars(params_.instrument_x_, bar.interval_);
		if (series_y == nullptr || series_x == nullptr) {
      return;
    }
    Eigen::VectorXd closes_y = series_y->column(analytics::BarField::CLOSE);
		Eigen::VectorXd closes_x = series_x->column(analytics::BarField::CLOSE);
		int sample_size = closes_x.size();
		// int trailed_size = sample_size - params_.lookback_;
		// if (sample_size != closes_y.size() || trailed_size <= 0) {
		// 	return;
		// }
		if (sample_size != closes_y.size() || sample_size < params_.lookback_) {
			return;
		}
		Eigen::VectorXd window_y = closes_y.tail(params_.lookback_);
		Eigen::VectorXd window_x = closes_x.tail(params_.lookback_);
		Eigen::Matrix<double, Eigen::Dynamic, 2> design(params_.lookback_, 2);
		design << window_x, Eigen::VectorXd::Ones(params_.lookback_);
		double hedge_ratio = statistics::fit_ols<Eigen::VectorXd, statistics::OlsAlgo::LLT>(window_y, design)[0];
		// Eigen::VectorXd hedge_ratio(trailed_size);
		// for (int i = 0; i < trailed_size; ++i) {
		// 	hedge_ratio[i] = statistics::fit_ols<Eigen::VectorXd, statistics::OlsAlgo::LLT>(
		// 		closes_y.segment(i, params_.lookback_),
		// 		closes_x.segment(i, params_.lookback_)
		// 	)[0];
		// }
		spread_.push_back(window_y[params_.lookback_ - 1] - hedge_ratio * window_x[params_.lookback_ - 1]);
		if (spread_.size() <= params_.lookback_) {
			return; // warm-up
		}
		Eigen::Map<const Eigen::VectorXd> spread(spread_.data() + spread_.size() - params_.lookback_, params_.lookback_);
		// Eigen::VectorXd spread = window_y.array() - hedge_ratio * window_x.array();
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
		// Eigen::VectorXd spread = closes_y.tail(trailed_size).array() - hedge_ratio.array() * closes_x.tail(trailed_size).array();
		// Eigen::VectorXd sma = analytics::sma(spread, params_.lookback_);
		// Eigen::VectorXd zscore = analytics::zscore(spread, params_.lookback_);
		// double spread = closes_y[sample_size] - hedge_ratio * closes_x[hedge_ratio];
		// 
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