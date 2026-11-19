#pragma once
#include "cachelet/eviction_policy.hpp"
#include <vector>
#include <string>
#include <unordered_set>
#include <set>
#include <unordered_map>

/**
 * Belady: an offline-only theoretically optimal policy, for the sake of benchmarking a policy against the best victim choice.
 */
namespace Cachelet {
    class BeladyPolicy : public EvictionPolicy {
        public:

        explicit BeladyPolicy(std::vector<std::size_t> next_accessed); // vector of when each item is NEXT accessed
        void on_access(const std::string &key) override;
        void on_insert(const std::string &key) override;
        void on_remove(const std::string &key, RemoveReason remove_reason) override;
        std::string victim() override;

        private:
        std::vector<std::size_t> next_accessed;
        std::size_t request_index = 0;
        // std::unordered_set<std::size_t> resident;
        // std::unordered_map<std::size_t, std::string> index_to_key;
        // auto cmp = [](const std::pair<std::size_t, std::string> &a, const std::pair<std::size_t, std::string> &b){
        //     return a.first < b.first;
        // };
        std::set<std::pair<std::size_t, std::string>> resident; // dont need comparator bc pair orders by first, then second by default
        std::unordered_map<std::string, std::size_t> key_to_next_access;
    };
}