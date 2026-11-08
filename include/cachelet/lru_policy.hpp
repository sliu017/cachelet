#pragma once
#include "cachelet/eviction_policy.hpp"
#include <list>
#include <string>
#include <unordered_map>

namespace Cachelet {
    class LruPolicy : public EvictionPolicy {
        public:
        void on_access(const std::string &key) override;
        void on_insert(const std::string &key) override;
        void on_remove(const std::string &key, RemoveReason remove_reason) override;
        std::string victim() override;

        private:
        std::list<std::string> recency_list;
        std::unordered_map<std::string, std::list<std::string>::iterator> key_to_node;
    };
}