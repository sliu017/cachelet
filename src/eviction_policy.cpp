#include "cachelet/eviction_policy.hpp"

#include "cachelet/policies/lru_policy.hpp"
#include "cachelet/policies/fifo_policy.hpp"

#include <memory>
#include <stdexcept>

std::unique_ptr<Cachelet::EvictionPolicy> Cachelet::make_policy(const std::string &name){
    if(name == "lru"){
        return std::make_unique<Cachelet::LruPolicy>();
    } else if(name == "fifo"){
        return std::make_unique<Cachelet::FifoPolicy>();
    } else { // add other policies here
        
    }
    throw std::invalid_argument("policy not recognized: " +name);
}
