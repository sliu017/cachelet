#include "cachelet/policies/belady_policy.hpp"
#include <cassert>
#include <algorithm>
#include <utility>

Cachelet::BeladyPolicy::BeladyPolicy(const std::vector<std::size_t> &next_accessed) :
    next_accessed(std::move(next_accessed)){}

void Cachelet::BeladyPolicy::on_access(const std::string &key){
    // update key's access time
    assert(key_to_next_access.contains(key));
    std::size_t old_index = key_to_next_access[key];
    key_to_next_access[key] = next_accessed[request_index]; // upd index of when the key was accessed
    resident.erase({old_index, key});
    resident.insert({next_accessed[request_index++], key});
}
void Cachelet::BeladyPolicy::on_insert(const std::string &key){
    assert(!key_to_next_access.contains(key));
    resident.insert({next_accessed[request_index], key});
    key_to_next_access.emplace(key, next_accessed[request_index++]);
}
void Cachelet::BeladyPolicy::on_remove(const std::string &key, Cachelet::BeladyPolicy::RemoveReason remove_reason){
    assert(key_to_next_access.contains(key));
    std::size_t index = key_to_next_access[key];
    resident.erase({index, key});
    key_to_next_access.erase(key);
}
std::string Cachelet::BeladyPolicy::victim(){
    assert(!resident.empty()); // undefined behavior for an empty cache
    std::string evicted_string;
    auto evicted_pair = resident.rbegin(); // evict the key that is "furthest away in the future", e.g.
    // has the most time until next access
    return evicted_pair->second;
}