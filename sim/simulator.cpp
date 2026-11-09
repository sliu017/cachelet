#include "simulator.hpp"

#include <unordered_set>
#include <cassert>

Cachelet::sim::SimResult Cachelet::sim::simulate(const std::vector<std::string> &requests, std::size_t capacity, EvictionPolicy &policy){
    return simulate(requests.begin(), requests.end(), capacity, policy);
}