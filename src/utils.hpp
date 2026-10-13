#pragma once

#include "algotradelib/src/core/types.hpp"
#include <algotradelib/core>

#include <chrono>
#include <string>

namespace strategy_replications {

namespace atl = algotradelib;

using atl::Bar;
using atl::BarInterval;
using atl::BarStamp;
using atl::Country;
using atl::Currency;
using atl::Decimal64;
using atl::Instrument;
using atl::InstrumentId;
using atl::PriceRule;
using atl::SecurityType;
using atl::Venue;

namespace utils {

// days_from_civil (https://howardhinnant.github.io/date_algorithms.html#days_from_civil)
inline std::chrono::system_clock::time_point days_from_civil(int yyyymmdd) {
  int y = yyyymmdd / 10000, m = yyyymmdd / 100 % 100, d = yyyymmdd % 100;
  y -= m <= 2;
  const int era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return std::chrono::system_clock::time_point{} + std::chrono::hours(24 * (era * 146097L + static_cast<long>(doe) - 719468L));
}

inline InstrumentId id_of(const std::string& name) {
	return InstrumentId{atl::Country::US, atl::Venue::COMBINED, name};
}

inline Instrument reference(const std::string& name) {
	Instrument instrument;
	instrument.id_ = id_of(name);
	instrument.name_ = name;
	instrument.type_ = SecurityType::STOCK;
	instrument.currency_ = Currency{"USD"};
	instrument.lot_size_ = Decimal64::parse("1");
  instrument.price_rule_ = PriceRule::unconstrained();
  return instrument;
}

inline Bar bar_of(int date, const std::string& close) {
	Bar bar;
	bar.timestamp_ = days_from_civil(date);
	bar.open_ = bar.high_ = bar.low_ = bar.close_ = Decimal64::parse(close);
	bar.volume_ = Decimal64::parse("100000");
	return bar;
}

} // namespace utils
} // namespace strategy_replications