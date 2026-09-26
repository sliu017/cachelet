#include "cachelet/cache.hpp"

#include <iterator>
#include <list>
#include <optional>
#include <unordered_map>
#include <string>


void Cachelet::Cache::set(std::string const &key, std::string const &value){
    auto existing_it = key_to_entry_map.find(key);
    if(existing_it != key_to_entry_map.end()){
        bring_entry_to_front(existing_it->second);
    } else {
        Entry to_ins = {
            to_ins.key = key,
            to_ins.value = value
        };
        lru_cache.push_front(to_ins);
        // TODO: EVICT HERE!
    }
    
}

std::optional<std::string> Cachelet::Cache::get(std::string const &key){
    auto it = cache_map.find(key);
    if(it != cache_map.end()){
        // found!
        return it->second;
    } else {
        // did not find, return nullptr-esque value that fits with the optional
        return std::nullopt;
    }
}

bool Cachelet::Cache::del(std::string const &key){
    auto it = cache_map.find(key);
    if(it != cache_map.end()){
        cache_map.erase(it);
        return true;
    } else {
        return false;
    }
}

std::size_t Cachelet::Cache::size() const{
    return cache_map.size();
}

void Cachelet::Cache::bring_entry_to_front(std::list<Entry>::iterator entry_it){
    // brings the specified entry to the start of the linked list, e.g. it is the most recently used
    lru_cache.splice(lru_cache.begin(), lru_cache, entry_it);
    // uses splice to efficiently move the target node to the beginning of the list
}
