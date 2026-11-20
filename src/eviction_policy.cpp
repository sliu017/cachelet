#include "cachelet/eviction_policy.hpp"

#include "cachelet/policies/lru_policy.hpp"
#include "cachelet/policies/fifo_policy.hpp"
#include "cachelet/policies/sieve_policy.hpp"
#include "cachelet/policies/s3fifo_policy.hpp"
#include "cachelet/policies/belady_policy.hpp"
#include "cachelet/policies/approx_lru_policy.hpp"

#include <memory>
#include <stdexcept>
#include <optional>
#include <vector>
#include <cstddef>

std::unique_ptr<Cachelet::EvictionPolicy> Cachelet::make_policy(const std::string &name, std::size_t capacity,
    const std::optional<std::vector<std::size_t>> &next_accessed_times){
    if(name == "lru"){
        return std::make_unique<Cachelet::LruPolicy>();
    } else if(name == "fifo"){
        return std::make_unique<Cachelet::FifoPolicy>();
    } else if(name == "sieve"){
        return std::make_unique<Cachelet::SievePolicy>();
    } else if(name == "s3fifov0"){
        return std::make_unique<Cachelet::S3FifoPolicy>(capacity); // NEED TO IMPLEMENT CAPACITY
    } else if(name == "belady"){
        return std::make_unique<Cachelet::BeladyPolicy>(std::move(*next_accessed_times));
    } else if(name == "approx-lru"){
        return std::make_unique<Cachelet::ApproxLruPolicy>(5, 1234567);
    } else { // add other policies here
        
    }
    throw std::invalid_argument("policy not recognized: " +name);
}
