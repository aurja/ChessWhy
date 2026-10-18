#include "chesswhy/cli.hpp"
#include "chesswhy/input.hpp"

#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

void write_error(std::ostream& err, std::string_view message) {
    err << "chesswhy: error: ";
    for (const char character : message) {
        if (character == '\n' || character == '\r') {
            err << ' ';
        } else {
            err << character;
        }
    }
    err << '\n';
}

} // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string_view> args;
    args.reserve(static_cast<std::size_t>(argc > 1 ? argc - 1 : 0));
    for (int index = 1; index < argc; ++index) {
        args.emplace_back(argv[index]);
    }

    try {
        const auto options = chesswhy::cli::parse_args(args);
        return chesswhy::cli::run(options, std::cout, std::cerr);
    } catch (const chesswhy::input::InputError& error) {
        write_error(std::cerr, error.what());
        return chesswhy::cli::kExitInput;
    } catch (const std::exception& error) {
        write_error(std::cerr, "internal error: " + std::string(error.what()));
        return chesswhy::cli::kExitEngine;
    }
}
