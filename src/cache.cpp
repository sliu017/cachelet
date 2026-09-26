#include "cachelet/cache.hpp"

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

void Cachelet::Cache::set(std::string const &key, std::string const &value){
    auto existing_it = key_to_entry_map.find(key);
    if(existing_it != key_to_entry_map.end()){
        // could possibly be overwritten, so replace the existing value
        existing_it->second->value = value;
        bring_entry_to_front(existing_it->second);
    } else {
        Entry to_ins = {
            .key = key,
            .value = value
        };
        lru_cache.push_front(to_ins);
        key_to_entry_map.insert_or_assign(key, lru_cache.begin());
        if(this->size() > this->MAX_ENTRIES){
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


