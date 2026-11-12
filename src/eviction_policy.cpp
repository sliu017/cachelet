#include "cachelet/eviction_policy.hpp"

#include "cachelet/policies/lru_policy.hpp"
#include "cachelet/policies/fifo_policy.hpp"
#include "cachelet/policies/sieve_policy.hpp"
#include "cachelet/policies/s3fifo_policy.hpp"

#include <memory>
#include <stdexcept>

std::unique_ptr<Cachelet::EvictionPolicy> Cachelet::make_policy(const std::string &name){
    if(name == "lru"){
        return std::make_unique<Cachelet::LruPolicy>();
    } else if(name == "fifo"){
        return std::make_unique<Cachelet::FifoPolicy>();
    } else if(name == "sieve"){
        return std::make_unique<Cachelet::SievePolicy>();
    } else if(name == "s3fifo"){
        // return std::make_unique<Cachelet::S3FifoPolicy>(); // NEED TO IMPLEMENT CAPACITY
    } else { // add other policies here
        
    }
    throw std::invalid_argument("policy not recognized: " +name);
}
