/**
 * @file data.hpp
 * @brief Loading csv files of the textbook
 * 
 * 
 */
#pragma once

#include <algotradelib/math>

// #include <iostream>
// #include <unordered_map>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace strategy_replications {

struct TimeSeriesData {
  std::vector<std::string> names_;
	// std::unordered_map<std::string, size_t> names_;
  std::vector<int> dates_; // yyyymmdd
	std::vector<std::vector<std::string>> cell_; // string types of values_.transpose()
  Eigen::MatrixXd values_;

	std::size_t col(const std::string& name) const {
		auto it = std::find(names_.begin(), names_.end(), name);
		if (it != names_.end()) {
			return it - names_.begin();
		}
		throw std::runtime_error("Could not find " + name);
	}

	void slice_dates(int from, int to) {
    const auto first = std::lower_bound(dates_.begin(), dates_.end(), from);
    const auto last = std::upper_bound(first, dates_.end(), to);
    // const auto begin = static_cast<Eigen::Index>(first - dates_.begin());
    // const auto count = static_cast<Eigen::Index>(last - first);
    // TimeSeriesData out;
    // out.names_ = df.names_;
    // out.dates_.assign(first, last);
    // out.values_ = df.values_.middleRows(begin, count);
    // out.cell_.reserve(df.cell_.size());
    // for (const auto& column : df.cell_) {
    //   out.cell_.emplace_back(column.begin() + begin, column.begin() + begin + count);
    // }
		const auto begin = first - dates_.begin();
		const auto end = last - dates_.begin();
		const auto count = end - begin;
		Eigen::MatrixXd kept = values_.middleRows(begin, count);
  	values_ = std::move(kept);
		for (auto& column : cell_) {
			column.erase(column.begin() + end, column.end());
			column.erase(column.begin(), column.begin() + begin);
		}
		dates_.erase(last, dates_.end());
		dates_.erase(dates_.begin(), dates_.begin() + begin);
	}
};

inline TimeSeriesData load_csv(const std::string& path) {
	std::ifstream file(path);
	if (!file.is_open()) {
		throw std::runtime_error("Cannot open " + path);
	}
	TimeSeriesData data_frame;
	std::string line, cell;
	// int num_cols = 0;
	if (std::getline(file, line)) {
		std::stringstream header(line);
		std::getline(header, cell, ',');
		while (std::getline(header, cell, ',')) {
			while (!cell.empty() && (cell.back() == '\r' || cell.back() == ' ')) {
				cell.pop_back();
			}
			data_frame.names_.push_back(cell);
			// data_frame.names_.emplace(cell, num_cols);
			// ++num_cols;
		}
	}
	int num_cols = data_frame.names_.size();
	data_frame.cell_.resize(num_cols);
	std::vector<double> vals;
	while (std::getline(file, line)) {
		if (line.empty()) {
			continue;
		}
		std::stringstream row(line);
		std::getline(row, cell, ',');
		data_frame.dates_.push_back(std::stoi(cell));
		for (int i = 0; i < num_cols; ++i) {
			cell.clear();
			std::getline(row, cell, ',');
			while (!cell.empty() && (cell.back() == '\r' || cell.back() == ' ')) {
				cell.pop_back();
			}
			const bool missing = cell.empty() || cell == "NaN" || cell == "nan";
			data_frame.cell_[i].push_back(missing ? std::string() : cell);
			vals.push_back(missing ? std::numeric_limits<double>::quiet_NaN() : std::stod(cell));
		}
	}
	int num_rows = data_frame.dates_.size();
	if (num_rows > 0 && num_cols > 0) {
		data_frame.values_ = Eigen::Map<Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>(vals.data(), num_rows, num_cols);
	}
	file.close();
	return data_frame;
}

inline TimeSeriesData load_ohlc(const std::string& directory, const std::string& kind) {
	auto df = load_csv(directory + "/inputDataOHLCDaily_20120424_" + kind + ".csv");
	std::ifstream file(directory + "/inputDataOHLCDaily_20120424_stocks.csv");
	std::string line;
	std::getline(file, line);
	std::getline(file, line);
	std::vector<std::string> names;
	std::size_t from = 0;
	while (true) {
		const auto to = line.find(',', from);
		auto cell = line.substr(from, to == std::string::npos ? std::string::npos : to - from);
		while (!cell.empty() && (cell.back() == '\r' || cell.back() == ' ')) {
			cell.pop_back();
		}
		names.push_back(cell);
		if (to == std::string::npos) {
			break;
		}
		from = to + 1;
	}
	df.names_ = names;
	return df;
}

} // namespace strategy_replications