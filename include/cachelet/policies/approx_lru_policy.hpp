#pragma once
#include "cachelet/eviction_policy.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <random>
#include <optional>
#include <cstdint>

/**
 * Exact LRU requires two pointers per element = 16 bytes, which could be considered wasteful. We will optimize
 * by storing a 4-byte integer per element tracking the last access point, then on eviction, sample a small number
 * of keys and evict the one that has the least last access index, i.e. the least recently used.
 * 
 * Improvements: Modern Redis uses an eviction pool that keeps track of good eviction candidates
 * instead of directly evicting the victim.
 */
namespace Cachelet {
    class ApproxLruPolicy : public EvictionPolicy {
        public:
        struct Entry {
            // unsigned access_index;
            std::uint64_t access_index; // NOTE: due to struct alignment we lose nothing from using a 64bit instead of a 32bit
            // since under a 32 bit, those 4 additional bytes would be padded anyway! (string is 8-byte padded)
            std::string key;
        };

        ApproxLruPolicy(std::size_t RANDOM_SAMPLE_SIZE, std::optional<std::uint32_t> seed);
        void on_access(const std::string &key) override;
        void on_insert(const std::string &key) override;
        void on_remove(const std::string &key, RemoveReason remove_reason) override;
        std::string victim() override;

        private:
        std::vector<Entry> resident;
        std::unordered_map<std::string, std::size_t> key_to_index; // maps a string, key, to its position in resident
        std::size_t RANDOM_SAMPLE_SIZE;
        std::size_t request_index = 0;
        std::mt19937 rng;
    };
}
