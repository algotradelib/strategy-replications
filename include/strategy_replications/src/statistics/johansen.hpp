#pragma once

#include <algotradelib/math>
#include "./ols.hpp"

#include <cmath>
#include <stdexcept>

namespace strategy_replications {
namespace statistics {

/** @brief The eigenvalues and eigenvectors of the Johansen test with a constant and one lagged difference. */
struct JohansenResult {
  Eigen::VectorXd eigenvalues_; // descending
  Eigen::MatrixXd eigenvectors_; // columns, normalized v' S_kk v = 1, first column's first entry positive
};

namespace detail {

inline Eigen::MatrixXd demean(const Eigen::MatrixXd& x) {
  return x.rowwise() - x.colwise().mean();
}

inline Eigen::MatrixXd residual(const Eigen::MatrixXd& y, const Eigen::MatrixXd& z) {
  return y - z * z.colPivHouseholderQr().solve(y);
}

} // namespace detail

/**
 * @brief The Johansen eigenvectors as `statsmodels.tsa.vector_ar.vecm.coint_johansen(x, 0, 1)` computes them
 *   (`det_order = 0`, `k_ar_diff = 1`), sign included.
 * @param x Observations in rows, series in columns.
 * @return The eigenvalues, descending, and the eigenvectors in the same order.
 */
inline JohansenResult johansen(const Eigen::MatrixXd& x) {
  const Eigen::Index n = x.rows();
  if (x.cols() < 1 || n < 4) {
    throw std::invalid_argument("johansen: at least one series and four rows");
  }
  const Eigen::MatrixXd level = detail::demean(x);
  const Eigen::MatrixXd dx = level.bottomRows(n - 1) - level.topRows(n - 1);
  const Eigen::MatrixXd z = detail::demean(dx.topRows(n - 2));
  const Eigen::MatrixXd r0 = detail::residual(detail::demean(dx.bottomRows(n - 2)), z);
  const Eigen::MatrixXd rk = detail::residual(detail::demean(level.middleRows(1, n - 2)), z);
  const double t = static_cast<double>(n - 2);
  const Eigen::MatrixXd skk = rk.transpose() * rk / t;
  const Eigen::MatrixXd sk0 = rk.transpose() * r0 / t;
  const Eigen::MatrixXd s00 = r0.transpose() * r0 / t;
  const Eigen::MatrixXd sig = sk0 * s00.ldlt().solve(sk0.transpose());
  const Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> solver(sig, skk);
  JohansenResult result;
  result.eigenvalues_ = solver.eigenvalues().reverse();
  result.eigenvectors_ = solver.eigenvectors().rowwise().reverse();
  if (result.eigenvectors_(0, 0) < 0) {
    result.eigenvectors_ = -result.eigenvectors_;
  }
  return result;
}

/**
 * @brief The half-life of mean reversion: `-log 2 / b` from the regression `dy ~ a + b y_{t-1}`.
 * @param y The series.
 * @return The half-life, in observations.
 */
inline double half_life(const Eigen::VectorXd& y) {
  const Eigen::Index n = y.size();
  Eigen::MatrixXd design(n - 1, 2);
  design << Eigen::VectorXd::Ones(n - 1), y.head(n - 1);
  const Eigen::VectorXd dy = y.tail(n - 1) - y.head(n - 1);
  return -std::log(2.0) / fit_ols<Eigen::VectorXd, OlsAlgo::QR>(dy, design)[1];
}

} // namespace statistics
} // namespace strategy_replications
