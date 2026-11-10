#include "cachelet/policies/lru_policy.hpp"
#include <cassert>

void Cachelet::LruPolicy::on_access(const std::string &key){
    assert(key_to_node.contains(key)); // by the interface conditions on calling this, the map MUST contain key
    auto map_it = key_to_node.find(key);
    std::list<std::string>::iterator list_it = map_it->second;
    recency_list.splice(recency_list.begin(), recency_list, list_it);
}

void Cachelet::LruPolicy::on_insert(const std::string &key){
    assert(!key_to_node.contains(key)); // should only be called when key is ABSENT
    recency_list.push_front(key);
    key_to_node.emplace(key, recency_list.begin()); // use emplace rather than insert_or_assign because this should always be an insert
}

void Cachelet::LruPolicy::on_remove(const std::string &key, [[maybe_unused]] RemoveReason remove_reason){
    assert(key_to_node.contains(key)); // should only be called when key is PRESENT
    auto it = key_to_node.find(key);
    recency_list.erase(it->second);
    key_to_node.erase(it);
}

std::string Cachelet::LruPolicy::victim(){
    assert(!recency_list.empty()); // undefined behavior for an empty list
    return recency_list.back(); // evict least recently used
}