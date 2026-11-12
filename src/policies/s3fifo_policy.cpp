#include "cachelet/policies/s3fifo_policy.hpp"
#include <cassert>
#include <algorithm>

Cachelet::S3FifoPolicy::S3FifoPolicy(std::size_t capacity) : 
    small_capacity(static_cast<std::size_t>(0.10 * capacity)), 
    main_target_size(static_cast<std::size_t>(capacity - small_capacity)), // See note in s3fifo_policy.hpp about main_target_size
    ghost_capacity(static_cast<std::size_t>(0.9 * capacity)){} // max cap. of small/ghost depend on passed in capacity so must def in ctor

void Cachelet::S3FifoPolicy::on_access(const std::string &key){ // Cache must contain key
    assert(key_to_list_entry.contains(key));
    auto map_it = key_to_list_entry.find(key);
    std::uint8_t freq = map_it->second.entry->freq;
    freq = std::min(freq + 1, 3); // increment with a cap at 3 
    map_it->second.entry->freq = freq; // update the frequency
}

void Cachelet::S3FifoPolicy::on_insert(const std::string &key){ // Cache must not contain key
    assert(!key_to_list_entry.contains(key));
    ListEntry new_list_entry = {
        .key = key,
        .freq = 0
    };
    bool is_main_queue = true;
    auto ghost_it = key_to_ghost_iter.find(key);
    if(ghost_it != key_to_ghost_iter.end()){ // we recently removed this but it's more popular than we thought, so promote straight to M
        main_queue.push_front(new_list_entry);
        // also remove it from ghost set, as libCacheSim does (see S3FIFO_find)
        ghost_queue.erase(ghost_it->second);
        key_to_ghost_iter.erase(ghost_it);
    } else { // otherwise "trial" the key in S
        small_queue.push_front(new_list_entry);
        is_main_queue = false;
    }
    MapEntry new_map_entry = {
        .which = is_main_queue ? Queue::Main : Queue::Small,
        .entry = is_main_queue ? main_queue.begin() : small_queue.begin()
    };
    key_to_list_entry.emplace(key, new_map_entry);
}

void Cachelet::S3FifoPolicy::on_remove(const std::string &key, RemoveReason remove_reason){ // Cache must contain key
    assert(key_to_list_entry.contains(key));
    auto map_it = key_to_list_entry.find(key);
    Queue which = map_it->second.which;
    if(which == Queue::Main){
        main_queue.erase(map_it->second.entry);
    } else {
        small_queue.erase(map_it->second.entry);
    }
    key_to_list_entry.erase(map_it);
    if(which == Queue::Small && remove_reason == RemoveReason::evicted){
        ghost_queue.push_front(key);
        key_to_ghost_iter.emplace(key, ghost_queue.begin());
        if(ghost_queue.size() > ghost_capacity){
            key_to_ghost_iter.erase(ghost_queue.back());
            ghost_queue.pop_back();
        }
        
    }
}

std::string Cachelet::S3FifoPolicy::victim(){ // Cache should not be empty
    assert(!key_to_list_entry.empty());
    if(small_queue.size() >= small_capacity){ // where the small queue's capacity is 10% of the overall cache capacity
        // evictS();
        while(small_queue.size() > 0){
            auto tail = std::prev(small_queue.end());
            if(tail->freq >= promote_to_main_threshold){ // as of now, >= 2 visits, but can be tweaked
                main_queue.splice(main_queue.begin(), small_queue, tail); // move S's tail out of small and at the head of main
                key_to_list_entry.at(tail->key).which = Queue::Main; // remember .at doesn't insert if missing, [] does
                if(main_queue.size() >= main_target_size){ // See note in s3fifo_policy.hpp about main_target_size
                    return evictM();
                }
            } else { // tail hasn't been visited enough :(
                return tail->key;
            }
        }
    } else {
        return evictM();
    }
    return evictM();
}

std::string Cachelet::S3FifoPolicy::evictM(){ // private helper
    assert(!main_queue.empty());
    while(main_queue.size() > 0){
        auto tail = std::prev(main_queue.end());
        if(tail->freq > 0){ // TODO: can make this 0 a parameter as well?
            main_queue.splice(main_queue.begin(), main_queue, tail);
            tail->freq -= 1;
        } else {
            return tail->key;
        }
    }
    assert(false && "unreachable"); // main_queue should always have size > 0, and the loop will always result in an eviction
    // since keys with freq > 0 are just sent back to the head and decremented
    return {};
}

/**
 *  private:
        // these generally mirror how they're implemented in libCacheSim. see:
        // https://github.com/1a1a11a/libCacheSim/blob/develop/libCacheSim/cache/eviction/S3FIFO.c
        std::size_t small_capacity; // = 0.1 * capacity;
        std::size_t main_target_size; // = capacity - small_queue_size;
        std::size_t ghost_capacity; // = 0.9 * capacity;
        std::size_t promote_to_main_threshold = 2; // freq >= threshold is the condition for promoting small->main

        enum class Queue {Small, Main};

        struct ListEntry {
            std::string key;
            std::uint8_t freq = 0; // should be capped at 3
        };
        struct MapEntry {
            Queue which;
            std::list<ListEntry>::iterator entry;
        };
        std::list<ListEntry> small_queue; // target size is small_capacity, evict from S when this is exceeded
        std::list<ListEntry> main_queue; // uses all the space small doesn't
        std::list<std::string> ghost_queue; // capped at 90% of total capacity

        std::unordered_map<std::string, MapEntry> key_to_list_entry; // maps to elements of small and main queues
        std::unordered_map<std::string, std::list<std::string>::iterator> key_to_ghost_iter; 
 */