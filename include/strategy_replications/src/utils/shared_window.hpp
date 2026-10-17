#pragma once

#include <algotradelib/analytics>

#include <cstddef>
#include <stdexcept>

namespace strategy_replications {

namespace atl = algotradelib;
namespace analytics = algotradelib::analytics;

namespace utils {

// The last `lookback_` closes on the dates both series share, oldest first; false until there are that many.
inline bool shared_window(
	const analytics::SeriesBuffer& y, const analytics::SeriesBuffer& x,
	Eigen::Ref<Eigen::VectorXd> window_y, Eigen::Ref<Eigen::VectorXd> window_x
) {
	const auto close = analytics::bar_field_index(analytics::BarField::CLOSE);
	std::ptrdiff_t i = static_cast<std::ptrdiff_t>(y.size()) - 1;
	std::ptrdiff_t j = static_cast<std::ptrdiff_t>(x.size()) - 1;
	std::ptrdiff_t filled = window_y.size();
	while (filled > 0 && i >= 0 && j >= 0) {
		const auto at_y = y.instant(static_cast<std::size_t>(i));
		const auto at_x = x.instant(static_cast<std::size_t>(j));
		if (at_y == at_x) {
			--filled;
			window_y[filled] = y.at(static_cast<std::size_t>(i), close);
			window_x[filled] = x.at(static_cast<std::size_t>(j), close);
			--i;
			--j;
		} else if (at_x < at_y) {
			--i;
		} else {
			--j;
		}
	}
	return filled == 0;
}

} // namespace utils
} // namespace strategy_replications