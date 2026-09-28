#pragma once // make sure only compiled once per build

#include <chrono>
#include <iterator>
#include <list>
#include <optional>
#include <unordered_map>
#include <string>

namespace Cachelet {
    class Cache {
        private:
        struct Entry {
            std::string key; // acts as a double-ended pathway to key_to_entry map
            std::string value;
            std::optional<std::chrono::steady_clock::time_point> expire_time; // Some keys might be set to never expire
        };
        // implemented through std::list, so no need to handle pointers ourselves
        std::list<Entry> lru_cache; // doubly linked list implementation
        std::unordered_map<std::string, std::list<Entry>::iterator> key_to_entry_map; // need an iterator to access the data!
        const size_t MAX_ENTRIES;

        void setHelper(std::string const &key, std::string const &value, 
            std::optional<std::chrono::steady_clock::duration> const &ttl);

        public:
        Cache(); // TODO: may want to consider removing this default constructor
        Cache(std::size_t max_entries); // constructor specifies the max number of entries it can hold
        void set(std::string const &key, std::string const &value);
        void set(std::string const &key, std::string const &value, std::chrono::steady_clock::duration const &ttl);
        std::optional<std::string> get(std::string const &key);
        bool del(std::string const &key);
        std::size_t size() const;
        
        void bring_entry_to_front(std::list<Entry>::iterator entry_it);
    };
}

