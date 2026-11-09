#pragma once

#include <string>
#include <memory>

namespace Cachelet {
    class EvictionPolicy {
        public:
        enum class RemoveReason { expired, deleted, evicted };
        // declare all as pure virtual because we dont want to encourage a "base behavior", this class should be abstract
        // these functions determine what the policy does in response to certian operations being performed
        virtual void on_access(const std::string &key) = 0; // runs whenever key is accessed

        // runs whenever key is inserted, policy doesnt care about the value 
        virtual void on_insert(const std::string &key) = 0; // NOTE: Might care about size in the future
        
        virtual void on_remove(const std::string &key, RemoveReason remove_reason) = 0; // runs whenever key is removed, for any reason
        virtual std::string victim() = 0; // determines the key to be evicted, returns it
        virtual ~EvictionPolicy() = default; // need VIRTUAL destructor bc nonvirtual dests will run on base class, not derived one
        // so if a class like LruPolicy needs extra data structures but ~EvictionPolicy is called on it, those DS will not be deleted
        
        EvictionPolicy() = default;
        EvictionPolicy(const EvictionPolicy&) = delete;
        EvictionPolicy& operator=(const EvictionPolicy&) = delete;
    };

    // name expected to be lowercase, e.g, "lru"
    std::unique_ptr<EvictionPolicy> make_policy(const std::string &name);
}