#pragma once
#include "cachelet/eviction_policy.hpp"
#include <string>
#include <queue>

namespace Cachelet {
    class FifoPolicy : public EvictionPolicy {
        public:
        void on_access(const std::string &key) override;
        void on_insert(const std::string &key) override;
        void on_remove(const std::string &key, RemoveReason remove_reason) override;
        std::string victim() override;

        private:
    };
}