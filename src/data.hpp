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
  Eigen::MatrixXd values_;
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

} // namespace strategy_replications