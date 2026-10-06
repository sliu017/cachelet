#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace Cachelet {
    namespace resp {
        enum class Status {Complete, Incomplete, Error};
        inline std::ostream& operator<<(std::ostream& os, Status s){
            switch(s) {
                case Status::Complete: return os << "Complete";
                case Status::Incomplete: return os << "Incomplete";
                case Status::Error: return os << "Error";
            }
            return os;
        }
        struct ParseResult {
            Status status = Status::Incomplete;
            std::size_t bytes_consumed = 0;
            std::vector<std::string> command;
        };
        ParseResult parse(const std::string &input);
        std::string encode_simple_string(const std::string &response);
        std::string encode_error(const std::string &response);
        std::string encode_integer(std::int64_t response);
        std::string encode_bulk_string(const std::string &response);
        std::string encode_null_bulk_string();
        
    };
}