#include "docoptexpr/docoptexpr.hh"

#include <cstdio>
#include <print>

using namespace docoptexpr::literals;

constexpr auto USAGE = R"(
Naval Fate.

Usage:
  naval_fate ship new <name>...
  naval_fate ship <name> move <x> <y> [--speed=<kn>]
  naval_fate ship shoot <x> <y>
  naval_fate mine set <x> <y> [--moored | --drifting]
  naval_fate mine remove <x> <y> [--moored | --drifting]
  naval_fate -h | --help
  naval_fate --version

Options:
  -h --help     Show this screen.
  --version     Show version.
  --speed=<kn>  Speed in knots [default: 10].
  --moored      Moored (anchored) mine.
  --drifting    Drifting mine.
)"_docopt;

auto main(int argc, char** argv) -> int {
	auto res = USAGE.parse(argc, argv);
	if(!res) {
		std::println(stderr, "Error matching arguments: {}\n", res.error());
		std::println(stderr, "{}", USAGE.usage());
		return 1;
	}

	auto val = res.value();

	if(val.get<"--help">()) {
		std::println("{}", USAGE.help());
		return 0;
	}
	if(val.get<"--version">()) {
		std::println("Naval Fate version 1.0");
		return 0;
	}

	if(val.get<"ship">()) {
		if(val.get<"new">()) {
			std::println("Creating new ships: {}", val.get<"<name>">());
		} else if(val.get<"move">()) {
			std::println("Moving ship {} to ({}, {}) at speed {} kn.", val.get<"<name>">(), val.get<"<x>">(), val.get<"<y>">(), val.get<"--speed">());
		} else if(val.get<"shoot">()) {
			std::println("Ship shooting at ({}, {}).", val.get<"<x>">(), val.get<"<y>">());
		}
	} else if(val.get<"mine">()) {
		auto mine_type = std::string{"unknown"};
		if(val.get<"--moored">()) {
			mine_type = "moored";
		}
		if(val.get<"--drifting">()) {
			mine_type = "drifting";
		}

		if(val.get<"set">()) {
			std::println("Setting {} mine at ({}, {}).", mine_type, val.get<"<x>">(), val.get<"<y>">());
		} else if(val.get<"remove">()) {
			std::println("Removing mine from ({}, {}).", val.get<"<x>">(), val.get<"<y>">());
		}
	}

	return 0;
}
