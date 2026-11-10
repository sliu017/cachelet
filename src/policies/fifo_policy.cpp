#include "cachelet/policies/fifo_policy.hpp"
#include <cassert>
#include <iterator> // for std::prev

void Cachelet::FifoPolicy::on_access([[maybe_unused]] const std::string &key){
    assert(key_to_node.contains(key)); // by the interface conditions on calling this, the map MUST contain key
    return; // in FIFO accessing doesn't do anything
}

void Cachelet::FifoPolicy::on_insert(const std::string &key){
    assert(!key_to_node.contains(key)); // should only be called when key is ABSENT
    queue.push_back(key);
    key_to_node.emplace(key, std::prev(queue.end())); // end() hits one past the last element, so we need to reach the previous one
}

void Cachelet::FifoPolicy::on_remove(const std::string &key, [[maybe_unused]] RemoveReason remove_reason){
    assert(key_to_node.contains(key)); // should only be called when key is PRESENT
    auto it = key_to_node.find(key);
    queue.erase(it->second);
    key_to_node.erase(it);
}

std::string Cachelet::FifoPolicy::victim(){
    assert(!queue.empty()); // undefined behavior for an empty list
    return queue.front(); // evict head of the queue
}