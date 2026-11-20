#include "cachelet/policies/approx_lru_policy.hpp"
#include <cstddef>
#include <random>
#include <cassert>
#include <utility>

Cachelet::ApproxLruPolicy::ApproxLruPolicy(std::size_t RANDOM_SAMPLE_SIZE, std::optional<std::uint32_t> seed) :
    RANDOM_SAMPLE_SIZE(RANDOM_SAMPLE_SIZE),
    rng(seed == std::nullopt ? std::random_device{}() : *seed){ // uses passed in seed if there was one
}

void Cachelet::ApproxLruPolicy::on_access(const std::string &key){
    assert(key_to_index.contains(key));
    std::size_t index = key_to_index[key];
    resident[index].access_index = request_index++;
}
void Cachelet::ApproxLruPolicy::on_insert(const std::string &key){
    assert(!key_to_index.contains(key));
    Entry new_entry = {
        .access_index = request_index++,
        .key = key
    };
    resident.push_back(new_entry);
    key_to_index[key] = resident.size() - 1; // key sits at the back of the vector
}
void Cachelet::ApproxLruPolicy::on_remove(const std::string &key, Cachelet::EvictionPolicy::RemoveReason remove_reason){
    assert(key_to_index.contains(key));
    std::size_t removed_key_index = key_to_index[key];
    std::swap(resident.back(), resident[removed_key_index]);
    std::string old_back_key = resident[removed_key_index].key; // the key of the elem that was previously at back()
    resident.pop_back(); // better time complexity than erasing from the middle of the vector, which can be O(n)
    // we simply swap the last element and the element we want to remove, which is O(1) b/c it just interchanges some pointers
    // then remove from the back of the vector, which is O(1)
    key_to_index[old_back_key] = removed_key_index; // update where the old back key now sits in resident
    key_to_index.erase(key);
}

std::string Cachelet::ApproxLruPolicy::victim(){
    
}

/**
 *      struct Entry {
            // unsigned access_index;
            std::uint64_t access_index; // NOTE: due to struct alignment we lose nothing from using a 64bit instead of a 32bit
            // since under a 32 bit, those 4 additional bytes would be padded anyway! (string is 8-byte padded)
            std::string key;
        };

 *      private:
        std::vector<Entry> resident;
        std::unordered_map<std::string, std::size_t> key_to_index; // maps a string, key, to its position in resident
        std::size_t RANDOM_SAMPLE_SIZE;
        std::size_t request_index = 0;
        std::mt19937 rng;
 */