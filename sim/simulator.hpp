#pragma once

#include <cassert>
#include <cstddef>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <limits>
#include "cachelet/eviction_policy.hpp"

namespace Cachelet::sim{
    struct SimResult {
        std::size_t hits = 0;
        std::size_t misses = 0;
    };
    // capacity == 0 will be rejected

    // transitioning to iterator pair template
    template <typename It>
    SimResult simulate(It first, It last, std::size_t capacity, EvictionPolicy &policy){
        assert(capacity > 0);
        std::unordered_set<std::string> resident;
        SimResult result;
        
        for(; first != last; ++first){
            const std::string& key = *first;  
            if(resident.find(key) == resident.end()){
                // not in the set, miss
                result.misses++;
                if(resident.size() >= capacity){ // evict
                    std::string vic = policy.victim();
                    resident.erase(vic);
                    policy.on_remove(vic, Cachelet::EvictionPolicy::RemoveReason::evicted);
                }
                policy.on_insert(key); // insert missed key
                resident.insert(key);

            } else {
                // in the set, hit!
                result.hits++;
                policy.on_access(key);
            }       
        }
        return result;
    }

    std::vector<std::size_t> compute_next_access_times(const std::vector<std::string> &requests);
    
    // keeping the vector overload so unit tests still work
    SimResult simulate(const std::vector<std::string> &requests, std::size_t capacity, EvictionPolicy &policy);
}