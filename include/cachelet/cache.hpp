#pragma once // make sure only compiled once per build

#include <optional>
#include <unordered_map>
#include <string>

namespace Cachelet {
    class Cache {
        private:
        std::unordered_map<std::string,std::string> cache_map;

        public:
        // just start with the fundamental two
        void set(std::string const &key, std::string const &value);
        std::optional<std::string> get(std::string const &key);
        bool del(std::string const &key);
    };
}

