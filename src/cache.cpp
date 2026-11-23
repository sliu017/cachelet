#include "cachelet/cache.hpp"

#include <chrono>
#include <iterator>
#include <limits>
#include <list>
#include <optional>
#include <unordered_map>
#include <ranges>
#include <string>
#include <stdexcept>

// construct the default constructor with the system integer limit (e.g. max unsigned value for a 64 bit int on my machine)
Cachelet::Cache::Cache() : MAX_ENTRIES(std::numeric_limits<std::size_t>::max()){

}
// need this funky way of constructing due to the const nature of MAX_ENTRIES
Cachelet::Cache::Cache(std::size_t max_entries) : MAX_ENTRIES(max_entries){
    if(this->MAX_ENTRIES <= 0){
        throw std::invalid_argument("Cache capacity must be at least 1!");
    }
}

// Set without a TTL
std::optional<std::string> Cachelet::Cache::set(std::string const &key, std::string const &value){
    return set_helper(key, value, std::nullopt);
}

// Set with a TTL
std::optional<std::string> Cachelet::Cache::set(std::string const &key, std::string const &value, 
    std::chrono::steady_clock::duration const &ttl){
    return set_helper(key, value, ttl);
}

// return an optional string value, representing the string that was evicted
// if no string was evicted, return nullopt
std::optional<std::string> Cachelet::Cache::set_helper(std::string const &key, std::string const &value, 
    std::optional<std::chrono::steady_clock::duration> const &ttl){
    auto existing_it = key_to_entry.find(key);
    std::optional<std::chrono::steady_clock::time_point> new_expire_time = std::nullopt;
    if(ttl != std::nullopt){
        new_expire_time = std::chrono::steady_clock::now() + ttl.value();
    }

    if(existing_it != key_to_entry.end()){ // Key exists?
        if(existing_it->second.expire_time != std::nullopt &&
        std::chrono::steady_clock::now() >= existing_it->second.expire_time){
            // key exists, but is expired
            key_to_entry.erase(key); // remove expired key from our map
            policy.on_remove(key, Cachelet::EvictionPolicy::RemoveReason::expired); // policy hook for removing a key
            // DON'T RETURN, we will treat the key as if it's an entirely new key in the logic outside this if statement
        } else {
            // key exists and has not expired
            // update value and ttl
            existing_it->second.value = value;
            existing_it->second.expire_time = new_expire_time;

            // policy hooks
            policy.on_access(key);
            return std::nullopt; // no eviction, since the key already exists we don't need to create room
        }
    }

    // Key not in cache
    Entry to_ins = {
        .value = value,
        .expire_time = new_expire_time
    };
    std::optional<std::string> key_to_erase = std::nullopt;
    if(this->size() >= this->MAX_ENTRIES){
        // max size reached, evict victim key based on policy
        key_to_erase = policy.victim();
        key_to_entry.erase(*key_to_erase);
        policy.on_remove(*key_to_erase, Cachelet::EvictionPolicy::RemoveReason::evicted);
    }
    // insert the new key
    key_to_entry.emplace(key, std::move(to_ins));
    policy.on_insert(key);

    return key_to_erase;
    
}

// rewritten to align with passing in an EvictionPolicy& p
std::optional<std::string> Cachelet::Cache::get(std::string const &key){
    auto it = key_to_entry.find(key);
    if(it != key_to_entry.end()){
        if(it->second.expire_time != std::nullopt &&
        std::chrono::steady_clock::now() >= it->second.expire_time){
            key_to_entry.erase(it);
            policy.on_remove(key, Cachelet::EvictionPolicy::RemoveReason::expired);
            return std::nullopt;
        }
        policy.on_access(key);
        return it->second.value;
    } else {
        // no hook, no cache state change
        return std::nullopt;
    }
}

bool Cachelet::Cache::del(std::string const &key){
    auto it = key_to_entry_map.find(key);
    if(it != key_to_entry_map.end()){
        lru_cache.erase(it->second);
        key_to_entry_map.erase(it);
        return true;
    } else {
        return false;
    }
}

std::size_t Cachelet::Cache::size() const{
    return lru_cache.size();
}

void Cachelet::Cache::bring_entry_to_front(std::list<Entry>::iterator entry_it){
    // brings the specified entry to the start of the linked list, e.g. it is the most recently used
    lru_cache.splice(lru_cache.begin(), lru_cache, entry_it);
    // uses splice to efficiently move the target node to the beginning of the list
}

void Cachelet::Cache::for_each(const std::function<void (const std::string &key, const std::string &value, 
                const std::optional<std::chrono::steady_clock::time_point> &expire)> &visit) const {
    // we can guarantee the correct cache contents but nothing about the policy state via this reconstruction
    // since we're just arbitrarily walking through all entries in the map
    for(const auto &[key, entry] : key_to_entry){
        visit(key, entry.value, entry.expire_time);
    }       
}


