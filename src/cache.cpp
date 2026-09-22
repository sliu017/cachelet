#include "cachelet/cache.hpp"

#include <optional>
#include <unordered_map>
#include <string>


void Cachelet::Cache::set(std::string const &key, std::string const &value){
    cache_map.insert_or_assign(key, value);
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
        cache_map.erase(key);
        return true;
    } else {
        return false;
    }
}
