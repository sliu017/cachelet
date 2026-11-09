#pragma once

#include <cstddef>
#include <vector>
#include <string>
#include "cachelet/eviction_policy.hpp"

namespace Cachelet::sim{
    struct SimResult {
        std::size_t hits = 0;
        std::size_t misses = 0;
    };
    // capacity <= 0 will be rejected
    SimResult simulate(const std::vector<std::string> &requests, std::size_t capacity, EvictionPolicy &policy);
}