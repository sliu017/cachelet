#pragma once // make sure only compiled once per build

#include <iterator>
#include <list>
#include <optional>
#include <unordered_map>
#include <string>

namespace Cachelet {
    class Cache {
        private:
        // std::unordered_map<std::string,std::string> cache_map; // no longer needed after we put our value in our Entry
        struct Entry {
            std::string key; // acts as a double-ended pathway to key_to_entry map
            std::string value;
            // implemented through std::list, so no need to handle pointers ourselves
        };
        std::list<Entry> lru_cache; // doubly linked list implementation
        std::unordered_map<std::string, std::list<Entry>::iterator> key_to_entry_map; // need an iterator to access the data!
        const size_t MAX_ENTRIES;

        public:
        Cache(); // TODO: may want to consider removing this default constructor
        Cache(std::size_t max_entries); // constructor specifies the max number of entries it can hold
        void set(std::string const &key, std::string const &value);
        std::optional<std::string> get(std::string const &key);
        bool del(std::string const &key);
        std::size_t size() const;
        
        void bring_entry_to_front(std::list<Entry>::iterator entry_it);
    };
}

