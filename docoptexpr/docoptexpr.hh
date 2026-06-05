#pragma once

#include <algorithm>
#include <array>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace docoptexpr {

namespace limits {
constexpr auto max_key_length       = size_t{64};
constexpr auto max_pattern_elements = size_t{16};
constexpr auto max_required_options = size_t{8};
} // namespace limits

// Helper to represent compile-time string literal NTTPs
template<size_t N>
struct literal_string {
	char data[N]{};

	constexpr literal_string(const char (&str)[N]) {
		std::copy_n(str, N, data);
	}

	constexpr auto view() const -> std::string_view {
		return std::string_view(data, N - 1);
	}
};

template<size_t N>
literal_string(const char (&str)[N]) -> literal_string<N>;

// Fixed-capacity string to use as NTTP inside std::array
template<size_t MaxLen = limits::max_key_length>
struct fixed_string {
	char   data[MaxLen]{};
	size_t length = 0;

	constexpr fixed_string() = default;

	template<size_t N>
	constexpr fixed_string(const char (&str)[N]) {
		static_assert(N <= MaxLen, "String exceeds maximum length!");
		std::copy_n(str, N - 1, data);
		length = N - 1;
	}

	constexpr fixed_string(std::string_view sv) {
		auto len = sv.size() < MaxLen ? sv.size() : MaxLen;
		for(auto i = size_t{0}; i < len; ++i) {
			data[i] = sv[i];
		}
		length = len;
	}

	constexpr auto view() const -> std::string_view {
		return std::string_view(data, length);
	}

	constexpr auto operator==(std::string_view sv) const -> bool {
		return view() == sv;
	}

	constexpr auto operator==(const fixed_string& other) const -> bool {
		return view() == other.view();
	}
};

enum class element_type { option,
	                        command,
	                        positional };

struct pattern_element {
	element_type                         type = element_type::command;
	fixed_string<limits::max_key_length> name{};
	bool                                 required  = true;
	bool                                 repeating = false;
};

struct pattern {
	std::array<pattern_element, limits::max_pattern_elements>                      elements{};
	size_t                                                                         element_count = 0;
	std::array<fixed_string<limits::max_key_length>, limits::max_required_options> required_options{};
	size_t                                                                         required_options_count = 0;
};

// Locale-independent ASCII helper functions
namespace ascii {
constexpr auto is_alpha(char c) -> bool {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

constexpr auto is_upper(char c) -> bool {
	return c >= 'A' && c <= 'Z';
}

constexpr auto to_lower(char c) -> char {
	if(c >= 'A' && c <= 'Z') {
		return c - 'A' + 'a';
	}
	return c;
}

constexpr auto is_digit(char c) -> bool {
	return c >= '0' && c <= '9';
}

constexpr auto is_alnum(char c) -> bool {
	return is_alpha(c) || is_digit(c);
}

constexpr auto starts_with_case_insensitive(std::string_view str, std::string_view prefix) -> bool {
	if(str.size() < prefix.size()) {
		return false;
	}
	for(auto i = size_t{0}; i < prefix.size(); ++i) {
		if(to_lower(str[i]) != to_lower(prefix[i])) {
			return false;
		}
	}
	return true;
}

constexpr auto trim(std::string_view str) -> std::string_view {
	while(!str.empty() && (str.front() == ' ' || str.front() == '\t' || str.front() == '\r' || str.front() == '\n')) {
		str.remove_prefix(1);
	}
	while(!str.empty() && (str.back() == ' ' || str.back() == '\t' || str.back() == '\r' || str.back() == '\n')) {
		str.remove_suffix(1);
	}
	return str;
}
} // namespace ascii

namespace detail {
struct section_offsets {
	size_t desc_start    = 0;
	size_t desc_len      = 0;
	size_t usage_start   = 0;
	size_t usage_len     = 0;
	size_t options_start = 0;
	size_t options_len   = 0;
};

constexpr auto find_section_offsets(std::string_view str) -> section_offsets {
	auto trimmed_str = ascii::trim(str);
	auto offsets     = section_offsets{};

	auto usage_pos   = std::string_view::npos;
	auto options_pos = std::string_view::npos;

	auto current_pos = size_t{0};
	while(current_pos < trimmed_str.size()) {
		auto next_line = trimmed_str.find('\n', current_pos);
		auto line_len  = (next_line == std::string_view::npos) ? (trimmed_str.size() - current_pos) : (next_line - current_pos);
		auto line      = trimmed_str.substr(current_pos, line_len);
		auto trimmed   = ascii::trim(line);

		if(ascii::starts_with_case_insensitive(trimmed, "usage:")) {
			usage_pos = current_pos;
		} else if(ascii::starts_with_case_insensitive(trimmed, "options:")) {
			options_pos = current_pos;
		}

		if(next_line == std::string_view::npos) {
			break;
		}
		current_pos = next_line + 1;
	}

	offsets.desc_start    = 0;
	auto first_header_pos = trimmed_str.size();
	if(usage_pos != std::string_view::npos && usage_pos < first_header_pos) {
		first_header_pos = usage_pos;
	}
	if(options_pos != std::string_view::npos && options_pos < first_header_pos) {
		first_header_pos = options_pos;
	}
	offsets.desc_len = first_header_pos;

	auto desc_view     = trimmed_str.substr(0, offsets.desc_len);
	desc_view          = ascii::trim(desc_view);
	offsets.desc_start = desc_view.data() - trimmed_str.data();
	offsets.desc_len   = desc_view.size();

	if(usage_pos != std::string_view::npos) {
		offsets.usage_start = usage_pos;
		auto usage_end      = trimmed_str.size();
		if(options_pos != std::string_view::npos && options_pos > usage_pos) {
			usage_end = options_pos;
		}
		offsets.usage_len = usage_end - usage_pos;

		auto usage_view     = trimmed_str.substr(offsets.usage_start, offsets.usage_len);
		usage_view          = ascii::trim(usage_view);
		offsets.usage_start = usage_view.data() - trimmed_str.data();
		offsets.usage_len   = usage_view.size();
	}

	if(options_pos != std::string_view::npos) {
		offsets.options_start = options_pos;
		auto options_end      = trimmed_str.size();
		if(usage_pos != std::string_view::npos && usage_pos > options_pos) {
			options_end = usage_pos;
		}
		offsets.options_len = options_end - options_pos;

		auto options_view     = trimmed_str.substr(offsets.options_start, offsets.options_len);
		options_view          = ascii::trim(options_view);
		offsets.options_start = options_view.data() - trimmed_str.data();
		offsets.options_len   = options_view.size();
	}

	return offsets;
}
} // namespace detail

// parse_result template storing the parsed states with compile-time checked keys
template<
  auto OptionShortNames,
  auto OptionLongNames,
  auto OptionHasArg,
  auto OptionDefaultValue,
  auto PositionalKeys,
  auto CommandKeys>
struct parse_result {
	std::array<bool, OptionShortNames.size()>             option_present{};
	std::array<std::string_view, OptionShortNames.size()> option_value{};

	std::array<bool, PositionalKeys.size()>             positional_present{};
	std::array<std::string_view, PositionalKeys.size()> positional_value{};

	std::array<bool, CommandKeys.size()> command_present{};

	// Compile-time verified getter
	template<literal_string Key>
	constexpr auto get() const -> auto {
		constexpr auto key_sv = Key.view();

		// Search in options
		constexpr auto opt_idx = [&]() -> int {
			for(auto i = size_t{0}; i < OptionShortNames.size(); ++i) {
				if((!OptionShortNames[i].view().empty() && OptionShortNames[i].view() == key_sv) || (!OptionLongNames[i].view().empty() && OptionLongNames[i].view() == key_sv)) {
					return int(i);
				}
			}
			return -1;
		}();

		if constexpr(opt_idx != -1) {
			constexpr auto has_arg = OptionHasArg[opt_idx];
			if constexpr(has_arg) {
				return option_present[opt_idx] ? option_value[opt_idx] : OptionDefaultValue[opt_idx].view();
			} else {
				return option_present[opt_idx];
			}
		}

		// Search in positionals
		constexpr auto pos_idx = [&]() -> int {
			for(auto i = size_t{0}; i < PositionalKeys.size(); ++i) {
				if(PositionalKeys[i].view() == key_sv) {
					return int(i);
				}
			}
			return -1;
		}();

		if constexpr(pos_idx != -1) {
			return positional_present[pos_idx] ? positional_value[pos_idx] : std::string_view{};
		}

		// Search in commands
		constexpr auto cmd_idx = [&]() -> int {
			for(auto i = size_t{0}; i < CommandKeys.size(); ++i) {
				if(CommandKeys[i].view() == key_sv) {
					return int(i);
				}
			}
			return -1;
		}();

		if constexpr(cmd_idx != -1) {
			return command_present[cmd_idx];
		}

		static_assert(opt_idx != -1 || pos_idx != -1 || cmd_idx != -1, "Key not found in docopt specifications!");
	}

	// Runtime query interfaces
	constexpr auto has(std::string_view key) const -> bool {
		for(auto i = size_t{0}; i < OptionShortNames.size(); ++i) {
			if((!OptionShortNames[i].view().empty() && OptionShortNames[i].view() == key) || (!OptionLongNames[i].view().empty() && OptionLongNames[i].view() == key)) {
				return option_present[i];
			}
		}
		for(auto i = size_t{0}; i < PositionalKeys.size(); ++i) {
			if(PositionalKeys[i].view() == key) {
				return positional_present[i];
			}
		}
		for(auto i = size_t{0}; i < CommandKeys.size(); ++i) {
			if(CommandKeys[i].view() == key) {
				return command_present[i];
			}
		}
		return false;
	}

	constexpr auto get_bool(std::string_view key) const -> bool {
		for(auto i = size_t{0}; i < OptionShortNames.size(); ++i) {
			if((!OptionShortNames[i].view().empty() && OptionShortNames[i].view() == key) || (!OptionLongNames[i].view().empty() && OptionLongNames[i].view() == key)) {
				return option_present[i];
			}
		}
		for(auto i = size_t{0}; i < CommandKeys.size(); ++i) {
			if(CommandKeys[i].view() == key) {
				return command_present[i];
			}
		}
		return false;
	}

	constexpr auto get_string(std::string_view key) const -> std::string_view {
		for(auto i = size_t{0}; i < OptionShortNames.size(); ++i) {
			if((!OptionShortNames[i].view().empty() && OptionShortNames[i].view() == key) || (!OptionLongNames[i].view().empty() && OptionLongNames[i].view() == key)) {
				return option_present[i] ? option_value[i] : OptionDefaultValue[i].view();
			}
		}
		for(auto i = size_t{0}; i < PositionalKeys.size(); ++i) {
			if(PositionalKeys[i].view() == key) {
				return positional_present[i] ? positional_value[i] : std::string_view{};
			}
		}
		return {};
	}
};

// Parser implementation
template<
  literal_string Str,
  auto           OptionShortNames,
  auto           OptionLongNames,
  auto           OptionHasArg,
  auto           OptionDefaultValue,
  auto           PositionalKeys,
  auto           CommandKeys,
  auto           Patterns>
struct docopt_parser {
	using result_type = parse_result<
	  OptionShortNames,
	  OptionLongNames,
	  OptionHasArg,
	  OptionDefaultValue,
	  PositionalKeys,
	  CommandKeys>;

	static constexpr auto offsets = detail::find_section_offsets(Str.view());

	constexpr auto help() const -> std::string_view {
		return ascii::trim(Str.view());
	}

	constexpr auto description() const -> std::string_view {
		return help().substr(offsets.desc_start, offsets.desc_len);
	}

	constexpr auto usage() const -> std::string_view {
		return help().substr(offsets.usage_start, offsets.usage_len);
	}

	constexpr auto options() const -> std::string_view {
		return help().substr(offsets.options_start, offsets.options_len);
	}

	constexpr auto parse(std::span<const std::string_view> args) const -> std::expected<result_type, std::string_view> {
		auto result = result_type{};

		// Set defaults
		for(auto i = size_t{0}; i < OptionShortNames.size(); ++i) {
			if(!OptionDefaultValue[i].view().empty()) {
				result.option_present[i] = true;
				result.option_value[i]   = OptionDefaultValue[i].view();
			}
		}

		auto positional_args = std::vector<std::string_view>{};
		positional_args.reserve(args.size());

		auto double_dash = false;

		for(auto i = size_t{0}; i < args.size(); ++i) {
			auto arg = args[i];

			if(double_dash) {
				positional_args.push_back(arg);
				continue;
			}

			if(arg == "--") {
				double_dash = true;
				continue;
			}

			if(arg.starts_with("--")) {
				auto eq       = arg.find('=');
				auto opt_name = (eq == std::string_view::npos) ? arg : arg.substr(0, eq);

				auto opt_idx = int{-1};
				for(auto j = size_t{0}; j < OptionLongNames.size(); ++j) {
					if(OptionLongNames[j].view() == opt_name) {
						opt_idx = int(j);
						break;
					}
				}

				if(opt_idx == -1) {
					return std::unexpected<std::string_view>("Unknown option");
				}

				result.option_present[opt_idx] = true;
				if(OptionHasArg[opt_idx]) {
					if(eq != std::string_view::npos) {
						result.option_value[opt_idx] = arg.substr(eq + 1);
					} else {
						if(i + 1 < args.size()) {
							result.option_value[opt_idx] = args[i + 1];
							i++;
						} else {
							return std::unexpected<std::string_view>("Missing argument for option");
						}
					}
				}
			} else if(arg.starts_with('-') && arg != "-") {
				auto cluster = arg.substr(1);
				for(auto char_idx = size_t{0}; char_idx < cluster.size(); ++char_idx) {
					auto c        = cluster[char_idx];
					auto opt_arr  = std::array<char, 3>{'-', c, '\0'};
					auto opt_name = std::string_view{opt_arr.data(), 2};

					auto opt_idx = int{-1};
					for(auto j = size_t{0}; j < OptionShortNames.size(); ++j) {
						if(OptionShortNames[j].view() == opt_name) {
							opt_idx = int(j);
							break;
						}
					}

					if(opt_idx == -1) {
						return std::unexpected<std::string_view>("Unknown short option");
					}

					result.option_present[opt_idx] = true;
					if(OptionHasArg[opt_idx]) {
						if(char_idx + 1 < cluster.size()) {
							result.option_value[opt_idx] = cluster.substr(char_idx + 1);
							break;
						} else {
							if(i + 1 < args.size()) {
								result.option_value[opt_idx] = args[i + 1];
								i++;
								break;
							} else {
								return std::unexpected<std::string_view>("Missing argument for short option");
							}
						}
					}
				}
			} else {
				positional_args.push_back(arg);
			}
		}

		// Try matching patterns
		for(auto p_idx = size_t{0}; p_idx < Patterns.size(); ++p_idx) {
			const auto& pat         = Patterns[p_idx];
			auto        temp_result = result;

			auto options_ok = true;
			for(auto ro_idx = size_t{0}; ro_idx < pat.required_options_count; ++ro_idx) {
				auto req_opt = pat.required_options[ro_idx].view();
				if(!temp_result.has(req_opt)) {
					options_ok = false;
					break;
				}
			}
			if(!options_ok) {
				continue;
			}

			if(match_pattern(0, 0, pat, positional_args, temp_result)) {
				return temp_result;
			}
		}

		return std::unexpected<std::string_view>("Arguments did not match any usage pattern");
	}

	constexpr auto parse(int argc, const char* const* argv) const -> std::expected<result_type, std::string_view> {
		auto args = std::vector<std::string_view>{};
		args.reserve(argc - 1);
		for(auto i = int{1}; i < argc; ++i) {
			args.push_back(std::string_view{argv[i]});
		}
		return parse(args);
	}

	constexpr auto operator()(int argc, const char* const* argv) const -> auto {
		return parse(argc, argv);
	}

private:
	constexpr auto match_pattern(
	  size_t                            elem_idx,
	  size_t                            arg_idx,
	  const pattern&                    pat,
	  std::span<const std::string_view> args,
	  result_type&                      result
	) const -> bool {
		while(elem_idx < pat.element_count && pat.elements[elem_idx].type == element_type::option) {
			elem_idx++;
		}

		if(arg_idx == args.size() && elem_idx == pat.element_count) {
			return true;
		}

		if(elem_idx == pat.element_count) {
			return false;
		}

		const auto& elem = pat.elements[elem_idx];

		if(arg_idx == args.size()) {
			for(auto i = elem_idx; i < pat.element_count; ++i) {
				if(pat.elements[i].type != element_type::option && pat.elements[i].required) {
					return false;
				}
			}
			return true;
		}

		auto arg = args[arg_idx];

		if(elem.type == element_type::command) {
			if(arg == elem.name.view()) {
				auto cmd_idx = int{-1};
				for(auto j = size_t{0}; j < CommandKeys.size(); ++j) {
					if(CommandKeys[j].view() == arg) {
						cmd_idx = int(j);
						break;
					}
				}
				if(cmd_idx != -1) {
					result.command_present[cmd_idx] = true;
				}

				if(match_pattern(elem_idx + 1, arg_idx + 1, pat, args, result)) {
					return true;
				}

				if(cmd_idx != -1) {
					result.command_present[cmd_idx] = false;
				}
			}

			if(!elem.required) {
				if(match_pattern(elem_idx + 1, arg_idx, pat, args, result)) {
					return true;
				}
			}
			return false;
		} else if(elem.type == element_type::positional) {
			auto pos_idx = int{-1};
			for(auto j = size_t{0}; j < PositionalKeys.size(); ++j) {
				if(PositionalKeys[j].view() == elem.name.view()) {
					pos_idx = int(j);
					break;
				}
			}

			if(elem.repeating) {
				// Match repeating positional by greedily taking remaining arguments
				auto max_matches = args.size() - arg_idx;
				for(auto num_matches = max_matches; num_matches >= 1; --num_matches) {
					if(pos_idx != -1) {
						result.positional_present[pos_idx] = true;
						result.positional_value[pos_idx]   = args[arg_idx]; // Point to the first matched argument
					}

					if(match_pattern(elem_idx + 1, arg_idx + num_matches, pat, args, result)) {
						return true;
					}

					if(pos_idx != -1) {
						result.positional_present[pos_idx] = false;
						result.positional_value[pos_idx]   = "";
					}
				}

				if(!elem.required) {
					if(match_pattern(elem_idx + 1, arg_idx, pat, args, result)) {
						return true;
					}
				}
				return false;
			} else {
				if(pos_idx != -1) {
					result.positional_present[pos_idx] = true;
					result.positional_value[pos_idx]   = arg;
				}

				if(match_pattern(elem_idx + 1, arg_idx + 1, pat, args, result)) {
					return true;
				}

				if(pos_idx != -1) {
					result.positional_present[pos_idx] = false;
					result.positional_value[pos_idx]   = "";
				}

				if(!elem.required) {
					if(match_pattern(elem_idx + 1, arg_idx, pat, args, result)) {
						return true;
					}
				}
				return false;
			}
		}

		return false;
	}
};

// Parser Compiler details
namespace detail {

struct temp_option {
	std::string_view short_name;
	std::string_view long_name;
	bool             has_arg = false;
	std::string_view arg_name;
	std::string_view default_value;
};

struct temp_pattern_element {
	element_type     type = element_type::command;
	std::string_view name;
	bool             required  = true;
	bool             repeating = false;
};

struct temp_pattern {
	std::vector<temp_pattern_element> elements;
};

struct temp_docopt {
	std::vector<temp_option>      options;
	std::vector<std::string_view> positionals;
	std::vector<std::string_view> commands;
	std::vector<temp_pattern>     patterns;
	std::string_view              program_name;
	std::string_view              docopt_string;
};

using ascii::trim;

constexpr auto split_lines(std::string_view str) -> std::vector<std::string_view> {
	auto lines = std::vector<std::string_view>{};
	auto i     = size_t{0};
	while(i < str.size()) {
		auto next_line = str.find('\n', i);
		if(next_line == std::string_view::npos) {
			lines.push_back(str.substr(i));
			break;
		}
		lines.push_back(str.substr(i, next_line - i));
		i = next_line + 1;
	}
	return lines;
}

constexpr auto tokenize(std::string_view str) -> std::vector<std::string_view> {
	auto tokens = std::vector<std::string_view>{};
	auto i      = size_t{0};
	while(i < str.size()) {
		while(i < str.size() && (str[i] == ' ' || str[i] == '\t' || str[i] == ',')) {
			i++;
		}
		if(i >= str.size()) {
			break;
		}

		auto start = i;
		if(str[i] == '<') {
			while(i < str.size() && str[i] != '>') {
				i++;
			}
			if(i < str.size()) {
				i++;
			}
		} else {
			while(i < str.size() && str[i] != ' ' && str[i] != '\t' && str[i] != ',') {
				i++;
			}
		}
		tokens.push_back(str.substr(start, i - start));
	}
	return tokens;
}

constexpr auto parse_option_line(std::string_view line) -> temp_option {
	auto trimmed_line = trim(line);
	auto desc_pos     = trimmed_line.find("  ");
	auto opts_part    = (desc_pos == std::string_view::npos) ? trimmed_line : trimmed_line.substr(0, desc_pos);
	auto desc_part    = (desc_pos == std::string_view::npos) ? "" : trimmed_line.substr(desc_pos);

	opts_part = trim(opts_part);
	desc_part = trim(desc_part);

	auto tokens = tokenize(opts_part);
	auto opt    = temp_option{};

	for(auto i = size_t{0}; i < tokens.size(); ++i) {
		auto tok = tokens[i];
		if(tok.starts_with("--")) {
			auto eq = tok.find('=');
			if(eq != std::string_view::npos) {
				opt.long_name = tok.substr(0, eq);
				opt.has_arg   = true;
				opt.arg_name  = tok.substr(eq + 1);
			} else {
				opt.long_name = tok;
				if(i + 1 < tokens.size() && !tokens[i + 1].starts_with('-')) {
					opt.has_arg  = true;
					opt.arg_name = tokens[i + 1];
					i++;
				}
			}
		} else if(tok.starts_with('-') && tok != "-") {
			auto eq = tok.find('=');
			if(eq != std::string_view::npos) {
				opt.short_name = tok.substr(0, eq);
				opt.has_arg    = true;
				opt.arg_name   = tok.substr(eq + 1);
			} else {
				opt.short_name = tok;
				if(i + 1 < tokens.size() && !tokens[i + 1].starts_with('-')) {
					opt.has_arg  = true;
					opt.arg_name = tokens[i + 1];
					i++;
				}
			}
		}
	}

	auto def_pos = desc_part.find("[default:");
	if(def_pos != std::string_view::npos) {
		auto def_val     = desc_part.substr(def_pos + 9);
		auto end_bracket = def_val.find(']');
		if(end_bracket != std::string_view::npos) {
			def_val = def_val.substr(0, end_bracket);
		}
		opt.default_value = trim(def_val);
	}

	return opt;
}

constexpr auto parse_options_section(const std::vector<std::string_view>& lines) -> std::vector<temp_option> {
	auto options    = std::vector<temp_option>{};
	auto in_options = false;
	for(auto line : lines) {
		auto trimmed = trim(line);
		if(trimmed.empty()) {
			continue;
		}

		if(ascii::starts_with_case_insensitive(trimmed, "options:")) {
			in_options         = true;
			auto colon_pos     = trimmed.find(':');
			auto opt_on_header = trim(trimmed.substr(colon_pos + 1));
			if(!opt_on_header.empty() && opt_on_header.starts_with('-')) {
				options.push_back(parse_option_line(opt_on_header));
			}
			continue;
		}

		if(in_options) {
			if(trimmed.starts_with('-')) {
				options.push_back(parse_option_line(line));
			} else {
				if(ascii::starts_with_case_insensitive(trimmed, "usage:")) {
					in_options = false;
				}
			}
		}
	}
	return options;
}

constexpr auto get_usage_lines(const std::vector<std::string_view>& lines) -> std::vector<std::string_view> {
	auto usage_lines = std::vector<std::string_view>{};
	auto in_usage    = false;
	for(auto line : lines) {
		auto trimmed = trim(line);
		if(trimmed.empty()) {
			if(in_usage) {
				in_usage = false;
			}
			continue;
		}

		if(ascii::starts_with_case_insensitive(trimmed, "usage:")) {
			in_usage               = true;
			auto colon_pos         = trimmed.find(':');
			auto pattern_on_header = trim(trimmed.substr(colon_pos + 1));
			if(!pattern_on_header.empty()) {
				usage_lines.push_back(pattern_on_header);
			}
			continue;
		}

		if(in_usage) {
			if(ascii::starts_with_case_insensitive(trimmed, "options:")) {
				in_usage = false;
			} else {
				usage_lines.push_back(trimmed);
			}
		}
	}
	return usage_lines;
}

constexpr auto tokenize_usage(std::string_view pattern_str) -> std::vector<std::string_view> {
	auto tokens = std::vector<std::string_view>{};
	auto i      = size_t{0};
	while(i < pattern_str.size()) {
		while(i < pattern_str.size() && (pattern_str[i] == ' ' || pattern_str[i] == '\t')) {
			i++;
		}
		if(i >= pattern_str.size()) {
			break;
		}

		auto c = pattern_str[i];
		if(c == '[' || c == ']' || c == '(' || c == ')' || c == '|') {
			tokens.push_back(pattern_str.substr(i, 1));
			i++;
			continue;
		}

		if(i + 2 < pattern_str.size() && pattern_str[i] == '.' && pattern_str[i + 1] == '.' && pattern_str[i + 2] == '.') {
			tokens.push_back(pattern_str.substr(i, 3));
			i += 3;
			continue;
		}

		auto start = i;
		if(c == '<') {
			while(i < pattern_str.size() && pattern_str[i] != '>') {
				i++;
			}
			if(i < pattern_str.size()) {
				i++;
			}
		} else {
			while(i < pattern_str.size() && pattern_str[i] != ' ' && pattern_str[i] != '\t' && pattern_str[i] != '[' && pattern_str[i] != ']' && pattern_str[i] != '(' && pattern_str[i] != ')' && pattern_str[i] != '|' && !(i + 2 < pattern_str.size() && pattern_str[i] == '.' && pattern_str[i + 1] == '.' && pattern_str[i + 2] == '.')) {
				i++;
			}
		}
		tokens.push_back(pattern_str.substr(start, i - start));
	}
	return tokens;
}

template<typename T>
constexpr auto vector_contains(const std::vector<T>& vec, const T& val) -> bool {
	for(const auto& item : vec) {
		if(item == val) {
			return true;
		}
	}
	return false;
}

constexpr auto parse_docopt_to_temp(std::string_view docopt_str) -> temp_docopt {
	auto temp          = temp_docopt{};
	auto trimmed_str   = trim(docopt_str);
	temp.docopt_string = trimmed_str;

	auto lines   = split_lines(trimmed_str);
	temp.options = parse_options_section(lines);

	auto usage_lines = get_usage_lines(lines);

	for(auto line : usage_lines) {
		auto tokens = tokenize_usage(line);
		if(tokens.empty()) {
			continue;
		}

		if(temp.program_name.empty()) {
			temp.program_name = tokens[0];
		}

		auto pat      = temp_pattern{};
		auto required = true;

		for(auto i = size_t{1}; i < tokens.size(); ++i) {
			auto tok = tokens[i];

			if(tok == "[") {
				required = false;
			} else if(tok == "]") {
				required = true;
			} else if(tok == "(" || tok == ")") {
				// grouping markers, optional support
			} else if(tok == "...") {
				if(!pat.elements.empty()) {
					pat.elements.back().repeating = true;
				}
			} else if(tok == "|") {
				// OR operator
			} else if(tok.starts_with('-')) {
				auto eq       = tok.find('=');
				auto opt_name = (eq == std::string_view::npos) ? tok : tok.substr(0, eq);

				auto opt_idx = int{-1};
				for(auto j = size_t{0}; j < temp.options.size(); ++j) {
					if(temp.options[j].short_name == opt_name || temp.options[j].long_name == opt_name) {
						opt_idx = int(j);
						break;
					}
				}

				auto elem     = temp_pattern_element{};
				elem.type     = element_type::option;
				elem.name     = opt_name;
				elem.required = required;
				pat.elements.push_back(elem);

				if(opt_idx == -1) {
					auto opt = temp_option{};
					if(opt_name.starts_with("--")) {
						opt.long_name = opt_name;
					} else {
						opt.short_name = opt_name;
					}
					if(eq != std::string_view::npos) {
						opt.has_arg  = true;
						opt.arg_name = tok.substr(eq + 1);
					}
					temp.options.push_back(opt);
				} else {
					if(eq != std::string_view::npos) {
						temp.options[opt_idx].has_arg  = true;
						temp.options[opt_idx].arg_name = tok.substr(eq + 1);
					}
				}
			} else {
				if(tok.starts_with('<') || (ascii::is_alpha(tok[0]) && ascii::is_upper(tok[0]))) {
					auto elem     = temp_pattern_element{};
					elem.type     = element_type::positional;
					elem.name     = tok;
					elem.required = required;
					pat.elements.push_back(elem);

					if(!vector_contains(temp.positionals, tok)) {
						temp.positionals.push_back(tok);
					}
				} else {
					auto elem     = temp_pattern_element{};
					elem.type     = element_type::command;
					elem.name     = tok;
					elem.required = required;
					pat.elements.push_back(elem);

					if(!vector_contains(temp.commands, tok)) {
						temp.commands.push_back(tok);
					}
				}
			}
		}
		temp.patterns.push_back(pat);
	}

	return temp;
}

struct docopt_counts {
	size_t num_options     = 0;
	size_t num_positionals = 0;
	size_t num_commands    = 0;
	size_t num_patterns    = 0;
};

template<
  size_t NumOptions,
  size_t NumPositionals,
  size_t NumCommands,
  size_t NumPatterns>
struct static_docopt_data {
	std::array<fixed_string<limits::max_key_length>, NumOptions> option_short_names{};
	std::array<fixed_string<limits::max_key_length>, NumOptions> option_long_names{};
	std::array<bool, NumOptions>                                 option_has_arg{};
	std::array<fixed_string<limits::max_key_length>, NumOptions> option_default_values{};

	std::array<fixed_string<limits::max_key_length>, NumPositionals> positional_keys{};
	std::array<fixed_string<limits::max_key_length>, NumCommands>    command_keys{};

	std::array<pattern, NumPatterns> patterns{};
};

template<literal_string Str>
struct parser_traits {
	static constexpr docopt_counts counts = []() -> docopt_counts {
		auto temp = parse_docopt_to_temp(Str.view());
		return docopt_counts{
		  .num_options     = temp.options.size(),
		  .num_positionals = temp.positionals.size(),
		  .num_commands    = temp.commands.size(),
		  .num_patterns    = temp.patterns.size()
		};
	}();

	using DataType = static_docopt_data<counts.num_options, counts.num_positionals, counts.num_commands, counts.num_patterns>;

	static constexpr DataType data = []() -> DataType {
		auto temp = parse_docopt_to_temp(Str.view());
		auto d    = DataType{};

		for(auto i = size_t{0}; i < temp.options.size(); ++i) {
			d.option_short_names[i]    = fixed_string<limits::max_key_length>(temp.options[i].short_name);
			d.option_long_names[i]     = fixed_string<limits::max_key_length>(temp.options[i].long_name);
			d.option_has_arg[i]        = temp.options[i].has_arg;
			d.option_default_values[i] = fixed_string<limits::max_key_length>(temp.options[i].default_value);
		}

		for(auto i = size_t{0}; i < temp.positionals.size(); ++i) {
			d.positional_keys[i] = fixed_string<limits::max_key_length>(temp.positionals[i]);
		}
		for(auto i = size_t{0}; i < temp.commands.size(); ++i) {
			d.command_keys[i] = fixed_string<limits::max_key_length>(temp.commands[i]);
		}

		for(auto i = size_t{0}; i < temp.patterns.size(); ++i) {
			auto pat          = pattern{};
			pat.element_count = temp.patterns[i].elements.size();
			for(auto j = size_t{0}; j < pat.element_count; ++j) {
				const auto& src           = temp.patterns[i].elements[j];
				pat.elements[j].type      = src.type;
				pat.elements[j].name      = fixed_string<limits::max_key_length>(src.name);
				pat.elements[j].required  = src.required;
				pat.elements[j].repeating = src.repeating;

				if(src.type == element_type::option && src.required) {
					pat.required_options[pat.required_options_count++] = fixed_string<limits::max_key_length>(src.name);
				}
			}
			d.patterns[i] = pat;
		}

		return d;
	}();
};

} // namespace detail

// Constexpr parser generator function
template<literal_string Str>
constexpr auto parse() -> auto {
	using Traits = detail::parser_traits<Str>;
	return docopt_parser<
	  Str,
	  Traits::data.option_short_names,
	  Traits::data.option_long_names,
	  Traits::data.option_has_arg,
	  Traits::data.option_default_values,
	  Traits::data.positional_keys,
	  Traits::data.command_keys,
	  Traits::data.patterns>{};
}

// Literal operator
inline namespace literals {
template<literal_string Str>
consteval auto operator""_docopt() -> auto {
	return docoptexpr::parse<Str>();
}
} // namespace literals

} // namespace docoptexpr
