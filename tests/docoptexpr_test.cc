#include "docoptexpr/docoptexpr.hh"

#include <cassert>
#include <print>

using namespace docoptexpr::literals;

// Confirm that argument matching can be evaluated entirely at compile time
constexpr auto test_compile_time() -> bool {
	constexpr auto parser = "Usage: my_program tcp <host> <port>"_docopt;
	constexpr auto args   = std::array<std::string_view, 3>{"tcp", "10.0.0.1", "443"};

	auto res = parser.parse(args);
	if(!res) {
		return false;
	}
	return res.value().get<"<host>">() == "10.0.0.1";
}

static_assert(test_compile_time(), "Compile-time matching failed!");

auto test_tcp_command() -> void {
	constexpr auto parser =
	  R"(Usage:
  my_program tcp <host> <port> [--timeout=<seconds>]
  my_program serial <port> [--baud=<rate>] [--timeout=<seconds>]
  my_program -h | --help

Options:
  -h, --help           Show this screen.
  --timeout=<seconds>  How long to wait [default: 10].
  --baud=<rate>        Baud rate [default: 9600].)"_docopt;

	// Test case 1: TCP command with default values
	{
		auto argv = std::array<const char*, 4>{"my_program", "tcp", "127.0.0.1", "8080"};
		auto res  = parser.parse(4, argv.data());
		assert(res.has_value());

		auto val = *res;
		assert(val.get<"tcp">() == true);
		assert(val.get<"serial">() == false);
		assert(val.get<"<host>">() == "127.0.0.1");
		assert(val.get<"<port>">() == "8080");
		assert(val.get<"--timeout">() == "10"); // checks default value
		assert(val.get<"--baud">() == "9600"); // checks default value
		assert(val.get<"--help">() == false);
	}

	// Test case 2: TCP command with custom timeout (long option with eq)
	{
		auto argv = std::array<const char*, 5>{"my_program", "tcp", "localhost", "9000", "--timeout=45"};
		auto res  = parser.parse(5, argv.data());
		assert(res.has_value());

		auto val = *res;
		assert(val.get<"tcp">() == true);
		assert(val.get<"<host>">() == "localhost");
		assert(val.get<"<port>">() == "9000");
		assert(val.get<"--timeout">() == "45");
	}

	// Test case 3: Serial command with custom options (separate options)
	{
		auto argv = std::array<const char*, 7>{"my_program", "serial", "COM3", "--baud", "115200", "--timeout", "5"};
		auto res  = parser.parse(7, argv.data());
		assert(res.has_value());

		auto val = *res;
		assert(val.get<"tcp">() == false);
		assert(val.get<"serial">() == true);
		assert(val.get<"<port>">() == "COM3");
		assert(val.get<"--baud">() == "115200");
		assert(val.get<"--timeout">() == "5");
	}

	// Test case 4: TCP command using non-const span of std::string_view
	{
		auto args      = std::array<std::string_view, 3>{"tcp", "127.0.0.1", "8080"};
		auto span_args = std::span<std::string_view>{args};
		auto res       = parser.parse(span_args);
		assert(res.has_value());

		auto val = *res;
		assert(val.get<"tcp">() == true);
		assert(val.get<"<host>">() == "127.0.0.1");
		assert(val.get<"<port>">() == "8080");
	}
}

auto test_options_and_failures() -> void {
	// Declaring a parser using constexpr function to show it is equivalent
	constexpr auto parser = docoptexpr::parse<
	  R"(Usage:
  my_program [-v] [--file=<path>] <input>
  my_program (-h | --help)

Options:
  -v, --verbose       Verbose logging.
  --file=<path>       Target file.
  -h, --help          Show help.)">();

	// Test case 1: Positional and optional verbose flag
	{
		auto argv = std::array<const char*, 3>{"my_program", "-v", "source.txt"};
		auto res  = parser.parse(3, argv.data());
		assert(res.has_value());

		auto val = *res;
		assert(val.get<"<input>">() == "source.txt");
		assert(val.get<"--verbose">() == true);
		assert(val.get<"-v">() == true);
		assert(val.get<"--file">() == "");
	}

	// Test case 2: Positional and option with value
	{
		auto argv = std::array<const char*, 4>{"my_program", "--file", "output.bin", "source.txt"};
		auto res  = parser.parse(4, argv.data());
		assert(res.has_value());

		auto val = *res;
		assert(val.get<"<input>">() == "source.txt");
		assert(val.get<"--verbose">() == false);
		assert(val.get<"--file">() == "output.bin");
	}

	// Test case 3: Failure on missing positional argument
	{
		auto argv = std::array<const char*, 2>{"my_program", "-v"};
		auto res  = parser.parse(2, argv.data());
		assert(!res); // Should fail because <input> is required and missing
	}

	// Test case 4: Help option matched
	{
		auto argv = std::array<const char*, 2>{"my_program", "--help"};
		auto res  = parser.parse(2, argv.data());
		assert(res.has_value());
		assert(res.value().get<"--help">() == true);
	}
}

auto test_long_arguments() -> void {
	constexpr auto parser = docoptexpr::parse<
	  R"(Usage:
  my_program --long-option=<value> <long-positional>

Options:
  --long-option=<value>  A very long option value.
)">();

	// Construct some really long strings (e.g. 500 characters)
	auto long_value      = std::string(500, 'a');
	auto long_positional = std::string(500, 'b');

	auto argv = std::array<std::string, 3>{
	  "my_program",
	  "--long-option=" + long_value,
	  long_positional
	};

	auto args = std::array<std::string_view, 2>{
	  argv[1],
	  argv[2]
	};

	auto res = parser.parse(args);
	assert(res.has_value());

	auto val = *res;
	assert(val.get<"--long-option">() == long_value);
	assert(val.get<"<long-positional>">() == long_positional);
}

// Confirm that help, description, options, and usage sections are correctly extracted at compile-time
constexpr auto test_help_methods_compile_time() -> bool {
	constexpr auto parser =
	  R"(

Naval Fate.

Usage:
  naval_fate ship new <name>...
  naval_fate -h | --help

Options:
  -h --help     Show this screen.

)"_docopt;

	return parser.description() == "Naval Fate." && parser.usage() == "Usage:\n  naval_fate ship new <name>...\n  naval_fate -h | --help" && parser.options() == "Options:\n  -h --help     Show this screen.";
}

static_assert(test_help_methods_compile_time(), "Section extraction failed!");

auto test_help_methods_runtime() -> void {
	assert(test_help_methods_compile_time());
}

auto main() -> int {
	test_tcp_command();
	test_options_and_failures();
	test_long_arguments();
	test_help_methods_runtime();
	std::println("All docoptexpr tests (compile-time & runtime) passed successfully!");
	return 0;
}
