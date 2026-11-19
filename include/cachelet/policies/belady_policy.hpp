#pragma once
#include "cachelet/eviction_policy.hpp"
#include <vector>
#include <string>

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
    };
}