#pragma once

#include <algotradelib/math>

namespace strategy_replications {

// namespace math = algotradelib::math;

namespace statistics {

enum class OlsAlgo {
	NORMAL,
	LLT,
	QR,
	SVD,
};

template <typename ResponseType = Eigen::VectorXd, OlsAlgo Algo = OlsAlgo::LLT>
inline ResponseType fit_ols(Eigen::Ref<ResponseType> y, Eigen::Ref<Eigen::MatrixXd> x) {
	if constexpr (Algo == OlsAlgo::NORMAL) {
		// beta_ = (x_.transpose() * x_).solve(x_.transpose() * y_);
		return (x.transpose() * x).ldlt().solve(x.transpose() * y);
	} else if constexpr (Algo == OlsAlgo::LLT) {
		return (x.transpose() * x).llt().solve(x.transpose() * y);
	} else if constexpr (Algo == OlsAlgo::QR) {
		return x.colPivHouseholderQr().solve(y);
	} else if constexpr (Algo == OlsAlgo::SVD) {
		// beta_ = x_.bdcSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(y_);
	}
}

/**
 * @brief Ordinary Least Squares
 * 
 * @tparam Algo Matrix decomposition method for the linear system
 * @tparam ResponseType 
 */
template <typename ResponseType = Eigen::VectorXd>
class Ols {
public:
	Ols(Eigen::Ref<const ResponseType> y, Eigen::Ref<const Eigen::MatrixXd> x)
	: y_(y), x_(x) {}
	virtual ~Ols() = default;

	void init(Eigen::Ref<const ResponseType> y, Eigen::Ref<const Eigen::MatrixXd> x, bool include_mean) {
		y_ = y;
		if (include_mean) {
			x_ = Eigen::MatrixXd(x.rows(), x.cols() + 1);
			x_ << x, Eigen::VectorXd::Ones(x.rows());
		} else {
			x_ = x;
		}
	}
	
	template <OlsAlgo Algo = OlsAlgo::LLT>
	void fit() {
		if constexpr (Algo == OlsAlgo::NORMAL) {
			// beta_ = (x_.transpose() * x_).solve(x_.transpose() * y_);
			beta_ = (x_.transpose() * x_).ldlt().solve(x_.transpose() * y_);
		} else if constexpr (Algo == OlsAlgo::LLT) {
			beta_ = (x_.transpose() * x_).llt().solve(x_.transpose() * y_);
		} else if constexpr (Algo == OlsAlgo::QR) {
			beta_ = x_.colPivHouseholderQr().solve(y_);
		} else if constexpr (Algo == OlsAlgo::SVD) {
			// beta_ = x_.bdcSvd(Eigen::ComputeThinU | Eigen::ComputeThinV).solve(y_);
		}
	}

	ResponseType beta() {
		return beta_;
	}

private:
	ResponseType y_, beta_;
	Eigen::MatrixXd x_;
};

} // namespace statistics
} // namespace strategy_replications