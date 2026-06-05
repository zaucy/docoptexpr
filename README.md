# docoptexpr

A header-only C++23 compile-time (`consteval`/`constexpr`) parser for [`docopt`](http://docopt.org/) with zero runtime memory allocations and compile-time key validation.

## Quick Start

```cpp
#include "docoptexpr/docoptexpr.hh"
#include <print>

using namespace docoptexpr::literals;

constexpr auto DOCOPT = R"(
My cool program!

Usage:
  my_prog tcp <host> <port> [--timeout=<seconds>]
  my_prog -h | --help

Options:
  -h --help           Show this screen.
  --timeout=<seconds> How long to wait [default: 10].
)"_docopt;

auto main(int argc, char** argv) -> int {

    auto res = DOCOPT.parse(argc, argv);
    if (!res) {
        std::println("Error: {}", res.error());
        return 1;
    }

    auto val = res.value();
    if (val.get<"--help">()) {
        std::println("{}", parser.help());
        return 0;
    }

    auto host = val.get<"<host>">();       // std::string_view
    auto port = val.get<"<port>">();       // std::string_view
    auto timeout = val.get<"--timeout">(); // std::string_view
    
    // val.get<"--invalid">(); // Compile Error: Key not found!
}
```

## Related projects

* [docopt.cpp](https://github.com/docopt/docopt.cpp)
* [docopt.c](https://github.com/docopt/docopt.c)

## License
MIT
