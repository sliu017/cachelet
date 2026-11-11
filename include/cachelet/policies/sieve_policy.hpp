#pragma once
#include "cachelet/eviction_policy.hpp"
#include <list>
#include <unordered_map>
#include <string>

namespace Cachelet {
    class SievePolicy : public EvictionPolicy {
        public:

        void on_access(const std::string &key) override;
        void on_insert(const std::string &key) override;
        void on_remove(const std::string &key, RemoveReason remove_reason) override;
        std::string victim() override;

        private:

        struct Entry {
            std::string key;
            bool visited;
        };
        std::list<Entry> queue;
        std::list<Entry>::iterator hand = queue.end(); // indicates "NULL"
        std::unordered_map<std::string, std::list<Entry>::iterator> key_to_entry;
    };
}