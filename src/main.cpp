#include "./kalman.hpp"
#include <CLI/CLI.hpp>
#include <iostream>
#include <map>
#include <string>

namespace {

enum class Strategy : int {
	BOLLINGER,
	KALMAN,
};

} // namespace

int main(int argc, char** argv) {
  CLI::App app{"Book examples"};
	Strategy strategy_tag;
	std::map<std::string, Strategy> strategy_map{
		{"bollinger", Strategy::BOLLINGER},
		{"kalman", Strategy::KALMAN}
	};
	std::string path;
	// std::string y_name, x_name;
	std::vector<std::string> variables;
	bool shorts{true};
	app.add_option("-s,--strategy", strategy_tag, "The strategy name")
		->required()
		->transform(CLI::CheckedTransformer(strategy_map, CLI::ignore_case));
	app.add_option("-d, --data", path, "Path for the csv file")->required();
	// app.add_option("-y, --yname", y_name, "")->required();
	// app.add_option("-x, --xname", x_name, "")->required();
	app.add_option("--vars", variables, "Variables names, can specify 2 for pair trading {y, x}")
		->delimiter(',')
		->allow_extra_args(false);
	app.add_flag("--no-shorts", shorts, "Do not use shorts if specified");
	CLI11_PARSE(app, argc, argv);
	switch (strategy_tag) {
		case Strategy::BOLLINGER:
			break;
		case Strategy::KALMAN:
			return strategy_replications::kalman(path, variables, shorts);
	}
  return 0;
}
