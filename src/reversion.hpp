#pragma once

#include <strategy_replications/reversion>
#include <strategy_replications/src/statistics/johansen.hpp>
#include "./backtesting.hpp"
#include "./utils.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace strategy_replications {

// Example 2.8 Linear Mean Reversion on a Johansen Portfolio
int johansen_linear(const std::string& path, std::vector<std::string> names, bool shorts) {
  auto df = load_csv(path);
  if (names.empty()) {
    names = df.names_;
  }
  Eigen::MatrixXd x(df.values_.rows(), static_cast<Eigen::Index>(names.size()));
  for (std::size_t i = 0; i < names.size(); ++i) {
    x.col(static_cast<Eigen::Index>(i)) = df.values_.col(static_cast<Eigen::Index>(df.col(names[i])));
  }
  const Eigen::VectorXd weights = statistics::johansen(x).eigenvectors_.col(0);
  const double half_life = statistics::half_life(x * weights);
  reversion::PortfolioReversion portfolio_reversion;
  for (std::size_t i = 0; i < names.size(); ++i) {
    portfolio_reversion.params_.instruments_.push_back(utils::id_of(names[i]));
    portfolio_reversion.params_.weights_.push_back(weights[static_cast<Eigen::Index>(i)]);
  }
  portfolio_reversion.params_.lookback_ = static_cast<std::size_t>(std::llround(half_life));
  const auto precision = std::cout.precision(17);
  std::cout << "eigenvector=";
  for (std::size_t i = 0; i < names.size(); ++i) {
    std::cout << (i == 0 ? "" : ",") << weights[static_cast<Eigen::Index>(i)];
  }
  std::cout << " half-life=" << half_life << " lookback=" << portfolio_reversion.params_.lookback_ << std::endl;
  std::cout.precision(precision);
  std::cout << "Both estimated over the whole sample: the figure carries look-ahead." << std::endl;
  backtesting::Config configs;
  configs.shorts_ = shorts;
  const auto tag = portfolio_reversion.params_.tag_; // read before the move
  const auto engine = backtesting::replay(std::move(portfolio_reversion), df, names, configs);
  const auto& result = engine.result();
  backtesting::replay_log(tag, result);
  backtesting::summarize(df, names, result, 0);
  return 0;
}

// Example 3.1 Trading Price Spread, Log Price Spread, and Ratio
template <reversion::LinearSpread Spread = reversion::LinearSpread::PRICE>
int linear(const std::string& path, const std::vector<std::string>& names, bool shorts) {
  auto df = load_csv(path);
  reversion::LinearReversion<Spread> linear_reversion;
  linear_reversion.params_.scale_ = (Spread == reversion::LinearSpread::PRICE) ? 100000 : 10000000;
  linear_reversion.params_.instrument_y_ = utils::id_of(names[0]);
  linear_reversion.params_.instrument_x_ = utils::id_of(names[1]);
  backtesting::Config configs;
  configs.shorts_ = shorts;
  const auto tag = linear_reversion.params_.tag_; // read before the move
  const auto engine = backtesting::replay(std::move(linear_reversion), df, names, configs);
  const auto& result = engine.result();
  backtesting::replay_log(tag, result);
  backtesting::summarize(df, names, result, 0);
  return 0;
}

// Example 3.2 Bollinger Band Mean Reversion Strategy
int bollinger(const std::string& path, const std::vector<std::string>& names, bool shorts) {
  auto df = load_csv(path);
  reversion::BollingerBand bollinger_band;
  bollinger_band.params_.scale_ = 1000;
  bollinger_band.params_.instrument_y_ = utils::id_of(names[0]);
  bollinger_band.params_.instrument_x_ = utils::id_of(names[1]);
  backtesting::Config configs;
  configs.shorts_ = shorts;
  const auto tag = bollinger_band.params_.tag_; // read before the move
  const auto engine = backtesting::replay(std::move(bollinger_band), df, names, configs);
  const auto& result = engine.result();
  backtesting::replay_log(tag, result);
  backtesting::summarize(df, names, result, 0);
  return 0;
}

// Example 3.3 Kalman Filter Mean Reversion Strategy
int kalman(const std::string& path, const std::vector<std::string>& names, bool shorts) {
  auto df = load_csv(path);
  reversion::KalmanReversion kalman_reversion;
  kalman_reversion.params_.scale_ = 1000;
  kalman_reversion.params_.instrument_y_ = utils::id_of(names[0]);
  kalman_reversion.params_.instrument_x_ = utils::id_of(names[1]);
  backtesting::Config configs;
  configs.shorts_ = shorts;
  const auto tag = kalman_reversion.params_.tag_; // read before the move
  const auto engine = backtesting::replay(std::move(kalman_reversion), df, names, configs);
  const auto& result = engine.result();
  backtesting::replay_log(tag, result);
  backtesting::summarize(df, names, result, 0);
  return 0;
}

// Example 4.3 Linear Long-Short Model
int longshort_daily(const std::string& directory, bool shorts) {
  auto df = load_ohlc(directory, "cl");
  df.slice_dates(20070103, 20111230);
  reversion::LinearLongShort linear_longshort;
  linear_longshort.params_.scale_ = 100000;
  linear_longshort.params_.universe_ = {};
  for (const auto& name : df.names_) {
    linear_longshort.params_.universe_.push_back(utils::id_of(name));
  }
  backtesting::Config configs;
  configs.shorts_ = shorts;
  const auto tag = linear_longshort.params_.tag_; // read before the move
  const auto engine = backtesting::replay(std::move(linear_longshort), df, df.names_, configs);
  const auto& result = engine.result();
  backtesting::replay_log(tag, result);
  backtesting::summarize(df, df.names_, result, 1);
  return 0;
}

} // namespace strategy_replications