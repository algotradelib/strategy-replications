#include "./reversion.hpp"
#include "./buy_on_gap.hpp"
#include "./longshort.hpp"
#include "./ts_mom.hpp"
#include <CLI/CLI.hpp>
#include <iostream>
#include <map>
#include <string>

namespace {

enum class Strategy : int {
	LINEAR,
	LINEAR_LOG,
	LINEAR_LONGSHORT,
	RATIO,
	BOLLINGER,
	KALMAN,
	BUYONGAP,
	LONGSHORT,
	TSMOM,
};

} // namespace

int main(int argc, char** argv) {
  CLI::App app{"Book examples"};
	Strategy strategy_tag;
	std::map<std::string, Strategy> strategy_map{
		{"linear", Strategy::LINEAR},
		{"linear-log", Strategy::LINEAR_LOG},
		{"linear-longshort", Strategy::LINEAR_LONGSHORT},
		{"ratio", Strategy::RATIO},
		{"bollinger", Strategy::BOLLINGER},
		{"kalman", Strategy::KALMAN},
		{"bog", Strategy::BUYONGAP},
		{"longshort", Strategy::LONGSHORT},
		{"tsmom", Strategy::TSMOM}
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
		case Strategy::LINEAR:
			return strategy_replications::linear<strategy_replications::reversion::LinearSpread::PRICE>(path, variables, shorts);
		case Strategy::LINEAR_LOG:
			return strategy_replications::linear<strategy_replications::reversion::LinearSpread::LOG>(path, variables, shorts);
		case Strategy::LINEAR_LONGSHORT:
			return strategy_replications::longshort<strategy_replications::LongshortFrequency::DAILY>(path);
		case Strategy::RATIO:
			return strategy_replications::linear<strategy_replications::reversion::LinearSpread::RATIO>(path, variables, shorts);
		case Strategy::BOLLINGER:
			return strategy_replications::bollinger(path, variables, shorts);
		case Strategy::KALMAN:
			return strategy_replications::kalman(path, variables, shorts);
		case Strategy::BUYONGAP:
			return strategy_replications::buy_on_gap(path);
		case Strategy::LONGSHORT:
			return strategy_replications::longshort<strategy_replications::LongshortFrequency::INTRADAY>(path);
		case Strategy::TSMOM:
			return strategy_replications::ts_mom(path, variables);
	}
  return 0;
}
