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
		R_t_.setZero();
		V_w_ = (delta / (1 - delta)) * Eigen::Matrix2d::Identity();
	}
	
	/**
	 * @brief Kalman Filter Recursions
	 * 
	 * @param y Measurement
	 * @param x Instrument
	 */
	void fit(Eigen::Ref<Eigen::VectorXd> y, Eigen::Ref<const Eigen::VectorXd> x) {
		int sample_size = y.size();
		for (int i = 0; i < sample_size; ++i) {
			on_predict(y[i], x[i]);
			on_correct(y[i]);
		}
	}

	math::Scalar forecast(const math::Scalar& y, const math::Scalar& x) {
		math::Scalar y_hat = y;
		on_predict(y_hat, x);
		on_correct(y_hat);
		return y_hat;
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
	void on_predict(math::Scalar& y_t, const math::Scalar& x_t) {
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
	void on_correct(math::Scalar& y_t) {
		Eigen::Vector2d K_t = R_t_ * x_t_ / Q_t_; // 3.13
		e_t_ = y_t - y_hat_;
		beta_ += K_t * e_t_;
		R_t_ -= K_t * x_t_.transpose() * R_t_;
	}
};

/**
 * @brief Kalman Filter for Multivariate Time Series
 * 
 * Transition equation: \f$z_t = B z_{t - 1} + F x_{t - 1} + w_{t - 1}\f$, \f$w_t \sim N(0, \Sigma_w)\f$
 * Measurement or observation equation: \f$y_t = H_t z_t + G x_t + v_t\f$, \f$v_t \sim N(0, \Sigma_v)\f$
 * 
 */
class MultivariateKf {
public:
	MultivariateKf() {}
	virtual ~MultivariateKf() = default;

	/**
	 * @brief Initialization
	 * 
	 * @param mu_0 
	 * @param Sigma_0 
	 */
	void init(Eigen::Ref<Eigen::VectorXd> mu_0, Eigen::Ref<Eigen::MatrixXd> Sigma_0) {
		state_ = mu_0;
		cov_state_ = Sigma_0;
	}

	/**
	 * @brief Kalman Filter Recursions
	 * 
	 * @param y 
	 */
	void fit(Eigen::Ref<Eigen::VectorXd> y) {
		for (int i = 0; i < T_; ++i) {
			on_predict(y);
			on_correct(y);
		}
	}

private:
	int T_;
	Eigen::VectorXd state_, instruments_, output_;
	Eigen::MatrixXd transition_mat_, transition_input_, observation_input_, measurement_mat_, gain_;
	Eigen::MatrixXd cov_state_, cov_output_, cov_transition_, cov_measurement_;
	Eigen::LLT<Eigen::MatrixXd> cov_output_llt_;

	/**
	 * @brief Prediction step
	 * 
	 * @param y_t 
	 */
	void on_predict(Eigen::Ref<Eigen::VectorXd> y_t) {
		output_ = y_t;
		state_ = transition_mat_ * state_ + transition_input_ * instruments_;
		cov_state_ = transition_mat_ * cov_state_ * transition_mat_.transpose() + cov_transition_;
		y_t = measurement_mat_ * state_ + observation_input_ * instruments_;
		cov_output_ = measurement_mat_ * cov_state_ * measurement_mat_.transpose() + cov_measurement_;
	}

	/**
	 * @brief Correction step
	 * 
	 * @param y_t 
	 */
	void on_correct(Eigen::Ref<Eigen::VectorXd> y_t) {
		// gain_ = cov_state_ * measurement_mat_.transpose() * cov_output_.inverse();
		Eigen::MatrixXd temp_mat = measurement_mat_ * cov_state_;
		cov_output_llt_.compute(cov_output_);
		// gain_ = cov_state_ * cov_output_llt_.solve(measurement_mat_).transpose();
		// state_ += gain_ * (y_t - output_);
		// cov_state_ += - gain_ * cov_output_ * gain_.transpose();
		cov_output_llt_.matrixL().solveInPlace(temp_mat);
		state_ += temp_mat.transpose() * cov_output_llt_.matrixL().solve(y_t - output_);
		cov_state_ -= temp_mat.transpose() * temp_mat;
	}
};

} // namespace reversion
} // namespace strategy_replications