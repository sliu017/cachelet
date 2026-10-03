#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace Cachelet {
    namespace resp {
        enum class Status {Complete, Incomplete, Error};
        struct ParseResult {
            Status status = Status::Incomplete;
            std::size_t bytes_consumed = 0;
            std::vector<std::string> command;
        };
        ParseResult parse(const std::string &input);
    };
}