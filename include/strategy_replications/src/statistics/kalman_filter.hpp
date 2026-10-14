#pragma once

#include "algotradelib/src/math/scalar.hpp"
#if !defined(ATL_USE_EIGEN)
	#define ATL_USE_EIGEN
#endif // ATL_USE_EIGEN

#if !defined(ATL_HAS_EIGEN)
	#define ATL_HAS_EIGEN
#endif // ATL_HAS_EIGEN

#include <algotradelib/analytics>
// #include <boost/random/normal_distribution.hpp>

namespace strategy_replications {

namespace math = algotradelib::math;

// using algotradelib::Decimal64;

namespace statistics {

/**
 * @brief Kalman Filter
 * 
 * Univariate Kalman Filter with identity transition.
 * 
 */
class KalmanFilter {
public:
	KalmanFilter() {}
	// KalmanFilter(const math::Scalar& delta = .0001, const math::Scalar& V_e = .001)
	// : y_hat_(0.0), Q_t_(0.0), V_e_(V_e),
	// 	beta_(Eigen::Vector2d::Zero()), x_t_(Eigen::Vector2d::Zero()),
	// 	R_t_(Eigen::Vector2d::Zero()),
	// 	V_w_((delta / (1 - delta)) * Eigen::Matrix2d::Identity()) {}

	virtual ~KalmanFilter() = default;

	/**
	 * @brief Initialization
	 * 
	 * @param mu_0 
	 * @param Sigma_0 
	 */
	void init(const math::Scalar& delta = .0001, const math::Scalar& V_e = .001) {
		y_hat_ = 0.0;
		Q_t_ = 0.0;
		V_e_ = V_e;
		beta_.setZero();
		x_t_.setZero();
		// R_t_.setZero();
		V_w_ = (delta / (1 - delta)) * Eigen::Matrix2d::Identity();
		R_t_ = -V_w_;
	}
	
	/**
	 * @brief Kalman Filter Recursions
	 * 
	 * @param y Measurement
	 * @param x Instrument
	 */
	void fit(Eigen::Ref<const Eigen::VectorXd> y, Eigen::Ref<const Eigen::VectorXd> x) {
		int sample_size = y.size();
		for (int i = 0; i < sample_size; ++i) {
			on_predict(x[i]);
			on_correct(y[i]);
		}
	}

	math::Scalar forecast(const math::Scalar& y, const math::Scalar& x) {
		on_predict(x);
		on_correct(y);
		return y_hat_;
	}

	math::Scalar hedge_ratio() {
		return beta_[0];
	}

	math::Scalar measurement_resid() {
		return e_t_;
	}

	math::Scalar measurement_sd() {
		return std::sqrt(Q_t_);
	}

private:
	math::Scalar y_hat_, Q_t_, V_e_, e_t_;
	Eigen::Vector2d beta_, x_t_;
	Eigen::Matrix2d R_t_, V_w_;
	
	/**
	 * @brief Prediction step
	 * 
	 * @param y_t Measurement at time t
	 * @param x_t Instrument at time t
	 */
	void on_predict(const math::Scalar& x_t) {
		x_t_[0] = x_t;
		x_t_[1] = 1.0;
		R_t_ += V_w_;
		y_hat_ = x_t_.dot(beta_);
		Q_t_ = x_t_.transpose() * R_t_ * x_t_ + V_e_;
	}

	/**
	 * @brief Correction step
	 * 
	 * @param y_t Measurement at time t
	 */
	void on_correct(const math::Scalar& y_t) {
		Eigen::Vector2d K_t = R_t_ * x_t_ / Q_t_; // 3.13
		e_t_ = y_t - y_hat_;
		beta_ += K_t * e_t_;
		R_t_ -= K_t * x_t_.transpose() * R_t_;
	}
};

} // namespace reversion
} // namespace strategy_replications