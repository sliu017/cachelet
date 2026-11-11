#include "cachelet/policies/sieve_policy.hpp"
#include <cassert>

void Cachelet::SievePolicy::on_access(const std::string &key){ // Cache must contain key
    assert(key_to_entry.contains(key));
    auto map_it = key_to_entry.find(key);
    map_it->second->visited = true;
}

void Cachelet::SievePolicy::on_insert(const std::string &key){ // Cache must not contain key
    assert(!key_to_entry.contains(key));
    queue.push_front({
        .key = key,
        .visited = false
    });
    key_to_entry.emplace(key, queue.begin());
}

void Cachelet::SievePolicy::on_remove(const std::string &key, [[maybe_unused]] RemoveReason remove_reason){ // Cache must contain key
    assert(key_to_entry.contains(key));
    auto map_it = key_to_entry.find(key);
    if(map_it->second == hand){
        if(hand == queue.begin()){ // avoid undefined behavior on decrementing .begin()
            hand = queue.end();
        } else { // only decrement when hand is not at the head, because if so we should keep it there to indicate "null"
            hand--; // move hand forward past the entry planned for removal. (where forward here is moving towards the head)
        }
    }
    queue.erase(map_it->second);
    key_to_entry.erase(map_it);

}

std::string Cachelet::SievePolicy::victim(){ // Cache should not be empty
    assert(!queue.empty());
    // evict the first object the hand lands on that doesn't have its visited bit set
    if(hand == queue.end()){
        hand = std::prev(queue.end());
    }
    while(hand->visited == true){
        hand->visited = false;
        // check for begin() before decrementing, since decrementing begin() is undefined
        if(hand == queue.begin()){
            hand = queue.end(); // can set to one past the last elem since we're decrementing anyway
        }
        hand--; // moving "forward" is moving towards the head, since that's where we insert new keys

    }
    return hand->key;
}