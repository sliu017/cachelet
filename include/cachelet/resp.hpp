#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace Cachelet {
    namespace resp {
        enum class Status {Complete, Incomplete, Error};
        struct ParseResult {
            Status status;
            std::size_t bytes_consumed;
            std::vector<std::string> command;
        };
        ParseResult parse(const std::string &input);
    };
}