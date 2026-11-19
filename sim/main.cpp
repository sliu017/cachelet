#include "simulator.hpp"
#include "cachelet/eviction_policy.hpp"

#include <iostream>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <string>
#include <memory>
#include <optional>

int main(int argc, char* argv[]){
    if(argc != 4){
        std::cerr << "expected format: " << argv[0] << " <trace> <policy> <capacity>\n"; // argv[0] required for invocation and is safe
        return 1;
    }

    std::size_t capacity;
    std::string capacity_string = argv[3];

    // Checks on capacity_string
    if(capacity_string.empty()){ // ensure capacity string not emmpty
        std::cerr << "expected argument for capacity\n";
        return 1;
    }
    if(!std::all_of(capacity_string.begin(), capacity_string.end(), // ensure capacity only includes digits, e.g. "-5" not allowed
        [](unsigned char c){ return std::isdigit(c); })){
            std::cerr << "capacity should only include digits\n";
            return 1;
    }
    try {
        std::size_t parsed_count = 0;
        capacity = std::stoull(capacity_string, &parsed_count); // pass into an unsigned long long
        if(parsed_count != capacity_string.size()){ // e.g. "123abc", stoull stops parsing on a
            std::cerr << "error reading capacity value\n";
            return 1;
        }
    } catch (const std::invalid_argument &e){ // not convertible into a size_t
        std::cerr << "capacity should be an integer greater than 0\n";
        return 1;
    } catch(const std::out_of_range &e){ // doesnt fit into a 64-bit size_t
        std::cerr << "capacity is too large\n";
        return 1;
    }
    // Validate the parsed capacity value
    if(capacity == 0){
        std::cerr << "capacity should be greater than 0\n";
        return 1;
    }

    // Parse selected policy
    std::string policy_string = argv[2];
    std::unique_ptr<Cachelet::EvictionPolicy> policy;
    try {
        if(policy_string != "belady"){
            policy = Cachelet::make_policy(policy_string, capacity, std::nullopt);
        }
    } catch(const std::invalid_argument &e){
        std::cerr << e.what() << "\n";
        return 1;
    }

    std::string trace_filepath = argv[1];
    std::ifstream trace(trace_filepath);
    if(!trace){
        std::cerr << "failed to open file at " << trace_filepath << "\n";
        return 1;
    }

    auto trace_iter = std::istream_iterator<std::string>(trace);
    Cachelet::sim::SimResult result;
    if(policy_string == "belady"){ // belady needs us to compute the next accessed times vector in O(n), so only do that if needed
        std::vector<std::string> requests(trace_iter, std::istream_iterator<std::string>{});
        std::vector<std::size_t> next_accessed_times = Cachelet::sim::compute_next_access_times(requests);
        policy = Cachelet::make_policy(policy_string, capacity, std::move(next_accessed_times)); // NAT is not used again after
        result = Cachelet::sim::simulate(requests, capacity, *policy);
    } else {
        result = Cachelet::sim::simulate(trace_iter, {}, capacity, *policy);
    }
    std::size_t total_requests = result.hits + result.misses;
    std::cout << "Results of running trace at " << trace_filepath << " on policy " << policy_string << ":\n";
    std::cout << "Cache hits: " << result.hits << "\n";
    std::cout << "Cache misses: " << result.misses << "\n";
    if(total_requests > 0){
        std::cout << "Cache miss ratio: " << static_cast<double>(result.misses) / total_requests << 
        " (" << result.misses << " / " << total_requests << ")\n";
    } else {
        std::cout << "Cache miss ratio: NaN (0 total requests)\n";
    }
    
    return 0;
}