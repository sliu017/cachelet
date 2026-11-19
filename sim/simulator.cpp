#include "simulator.hpp"

#include <unordered_set>
#include <cassert>

Cachelet::sim::SimResult Cachelet::sim::simulate(const std::vector<std::string> &requests, std::size_t capacity, EvictionPolicy &policy){
    return simulate(requests.begin(), requests.end(), capacity, policy);
}

std::vector<std::size_t> Cachelet::sim::compute_next_access_times(const std::vector<std::string> &requests){
        std::vector<std::size_t> next_access_time(requests.size());
        std::unordered_map<std::string, std::size_t> last_accessed; // technically "next" accessed since it's a backward pass
        // but for the sake of logic we'll call it last
        for(std::size_t i = requests.size(); i-- > 0;){ // i >= 0 always true for unsigned
            const std::string &key = requests[i];
            if(last_accessed.contains(key)){
                next_access_time[i] = last_accessed[key];
            } else {
                next_access_time[i] = std::numeric_limits<std::size_t>::max(); // standin for INF
            }
            last_accessed[key] = i;
        }
        return next_access_time;
}