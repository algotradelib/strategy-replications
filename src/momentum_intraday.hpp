#pragma once

#include <strategy_replications/src/momentum/opening_gap.hpp>
#include <strategy_replications/src/momentum/post_earnings_drift.hpp>
#include "./backtesting_intraday.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace strategy_replications {

namespace detail {

// One column of a `Date,Open,High,Low,Close` file, as a one-instrument frame named `name`.
inline TimeSeriesData column_of(const TimeSeriesData& file, const std::string& field, const std::string& name) {
  TimeSeriesData out;
  out.names_ = {name};
  out.dates_ = file.dates_;
  out.cell_ = {file.cell_[file.col(field)]};
  out.values_ = file.values_.col(static_cast<Eigen::Index>(file.col(field)));
  return out;
}

} // namespace detail

// Example 7.1 Opening Gap Strategy for FSTX
int opening_gap(const std::string& path, bool shorts) {
  const auto file = load_csv(path);
  const std::string name = "FSTX";
  auto open = detail::column_of(file, "Open", name);
  auto high = detail::column_of(file, "High", name);
  auto low = detail::column_of(file, "Low", name);
  auto close = detail::column_of(file, "Close", name);
  momentum::OpeningGap gap_model;
  gap_model.params_.instrument_ = utils::id_of(name);
  backtesting::Config configs;
  configs.cash_ = "1000000000"; // the capital, so that the first day's return is zero
  configs.shorts_ = shorts;
  const double capital = gap_model.params_.capital_; // read before the move
  const auto tag = gap_model.params_.tag_; // read before the move
  const auto engine = backtesting::replay_intraday(std::move(gap_model), open, high, low, close, configs);
  const auto& result = engine.result();
  backtesting::replay_log(tag, result);
  backtesting::summarize_intraday(close, result, capital, 0);
  return 0;
}

// Post-Earnings Announcement Drift (chapter 7), over the dates of earnannFile.csv
int pead(const std::string& directory, bool shorts) {
  auto open = load_ohlc(directory, "op");
  auto high = load_ohlc(directory, "hi");
  auto low = load_ohlc(directory, "lo");
  auto close = load_ohlc(directory, "cl");
  const auto announced = load_csv(directory + "/earnannFile.csv");
  const int from = announced.dates_.front();
  const int to = announced.dates_.back();
  open.slice_dates(from, to);
  high.slice_dates(from, to);
  low.slice_dates(from, to);
  close.slice_dates(from, to);
  if (close.dates_ != announced.dates_ || close.names_ != announced.names_) {
    throw std::runtime_error("earnannFile.csv has other dates or stocks than the price files");
  }
  momentum::PostEarningsDrift pead_model;
  for (const auto& name : close.names_) {
    pead_model.params_.universe_.push_back(utils::id_of(name));
  }
  for (std::size_t day = 0; day < announced.dates_.size(); ++day) {
    Date date;
    date.year_ = announced.dates_[day] / 10000;
    date.month_ = static_cast<unsigned>(announced.dates_[day] / 100 % 100);
    date.day_ = static_cast<unsigned>(announced.dates_[day] % 100);
    std::vector<bool> flags(announced.names_.size());
    for (std::size_t at = 0; at < flags.size(); ++at) {
      flags[at] = announced.values_(static_cast<Eigen::Index>(day), static_cast<Eigen::Index>(at)) != 0;
    }
    pead_model.params_.announced_.emplace(date, std::move(flags));
  }
  backtesting::Config configs;
  configs.cash_ = "1000000000"; // the capital, so that the first day's return is zero
  configs.shorts_ = shorts;
  const double capital = pead_model.params_.capital_; // read before the move
  const auto tag = pead_model.params_.tag_; // read before the move
  const auto engine = backtesting::replay_intraday(std::move(pead_model), open, high, low, close, configs);
  const auto& result = engine.result();
  backtesting::replay_log(tag, result);
  backtesting::summarize_intraday(close, result, capital, 0);
  return 0;
}

} // namespace strategy_replications
