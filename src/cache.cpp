#include "cachelet/cache.hpp"

#include <chrono>
#include <iterator>
#include <limits>
#include <list>
#include <optional>
#include <unordered_map>
#include <string>

// construct the defualt constructor with the system integer limit (e.g. max unsigned value for a 64 bit int on my machine)
Cachelet::Cache::Cache() : MAX_ENTRIES(std::numeric_limits<std::size_t>::max()){

}
// need this funky way of constructing due to the const nature of MAX_ENTRIES
Cachelet::Cache::Cache(std::size_t max_entries) : MAX_ENTRIES(max_entries){

}

// Set without a TTL
void Cachelet::Cache::set(std::string const &key, std::string const &value){
    setHelper(key, value, std::nullopt);
}

// Set with a TTL
void Cachelet::Cache::set(std::string const &key, std::string const &value, std::chrono::steady_clock::duration const &ttl){
    setHelper(key, value, ttl);
}

void Cachelet::Cache::setHelper(std::string const &key, std::string const &value, 
    std::optional<std::chrono::steady_clock::duration> const &ttl){
    auto existing_it = key_to_entry_map.find(key);
    std::optional<std::chrono::steady_clock::time_point> new_expire_time = std::nullopt;
    if(ttl != std::nullopt){
        new_expire_time = std::chrono::steady_clock::now() + ttl.value();
    }

    if(existing_it != key_to_entry_map.end()){
        // could possibly be overwritten, so replace the existing value
        existing_it->second->value = value;
        // update the expiry time with the new ttl
        existing_it->second->expire_time = new_expire_time;
        bring_entry_to_front(existing_it->second);
    } else {
        Entry to_ins = {
            .key = key,
            .value = value,
            .expire_time = new_expire_time
        };
        // move to front of LRU cache since this entry was most recently accessed
        lru_cache.push_front(to_ins);
        // add to key_to_entry_map to allow quick access of the entry in LRU cache
        key_to_entry_map.insert_or_assign(key, lru_cache.begin());
        if(this->size() > this->MAX_ENTRIES){
            // max size reached, evict least recently used
            std::string key_to_erase = lru_cache.back().key;
            lru_cache.pop_back();
            key_to_entry_map.erase(key_to_erase);
        }
    }
    
}

std::optional<std::string> Cachelet::Cache::get(std::string const &key){
    auto it = key_to_entry_map.find(key);
    if(it != key_to_entry_map.end()){
        // found!

        // check for expiry
        if(it->second->expire_time != std::nullopt &&
            std::chrono::steady_clock::now() >= it->second->expire_time){
            // expired
            del(key); 
            return std::nullopt; // expirations and misses are returned as the same - matches redis
        }
        bring_entry_to_front(it->second);
        return it->second->value;
    } else {
        // did not find, return nullptr-esque value that fits with the optional
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


