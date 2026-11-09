#include "simulator.hpp"

#include <unordered_set>
#include <cassert>

Cachelet::sim::SimResult Cachelet::sim::simulate(const std::vector<std::string> &requests, std::size_t capacity, EvictionPolicy &policy){
    assert(capacity > 0); // zero or negative capacity cache is treated as undefined
    std::unordered_set<std::string> resident; // all keys that are in the cache
    std::size_t hits = 0;
    std::size_t misses = 0;
    for(const std::string &key : requests){
        if(resident.find(key) == resident.end()){
            // not in the set, miss
            misses++;
            if(resident.size() >= capacity){ // evict
                std::string vic = policy.victim();
                resident.erase(vic);
                policy.on_remove(vic, Cachelet::EvictionPolicy::RemoveReason::evicted);
            }
            policy.on_insert(key); // insert missed key
            resident.insert(key);

        } else {
            // in the set, hit!
            hits++;
            policy.on_access(key);
        }
    }
    return SimResult{
        .hits = hits,
        .misses = misses
    };
}