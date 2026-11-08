#include <gtest/gtest.h>
#include "cachelet/lru_policy.hpp"

#include <string>
#include <vector>

using namespace Cachelet;
using Cachelet::LruPolicy;

namespace {
    std::vector<std::string> drain(EvictionPolicy &p, std::size_t size){
        std::vector<std::string> ret;
        while(size--){
            std::string vic = p.victim();
            ret.push_back(vic);
            p.on_remove(vic, EvictionPolicy::RemoveReason::evicted);
        }
        return ret;
    }
}

TEST(LRUTest, OneKey){
    LruPolicy p;
    p.on_insert("Miami");
    EXPECT_EQ(p.victim(), "Miami");
}

TEST(LRUTest, InsertionOrder){
    LruPolicy p;
    p.on_insert("Lakeland");
    p.on_insert("Tampa");
    p.on_insert("Port St. Lucie");
    EXPECT_EQ(drain(p,3), (std::vector<std::string>{"Lakeland", "Tampa", "Port St. Lucie"}));
}

TEST(LRUTest, AccessChangesInsertionOrder){
    LruPolicy p;
    p.on_insert("Santa Ines");
    p.on_insert("Bacabal");
    p.on_insert("Sao Luis");
    p.on_access("Bacabal");
    EXPECT_EQ(drain(p,3), (std::vector<std::string>{"Santa Ines", "Sao Luis", "Bacabal"}));
}

TEST(LRUTest, RemoveFromMiddle){
    LruPolicy p;
    p.on_insert("Phoenix");
    p.on_insert("Tempe");
    p.on_insert("Mesa");
    p.on_remove("Tempe", EvictionPolicy::RemoveReason::deleted);
    EXPECT_EQ(drain(p,2), (std::vector<std::string>{"Phoenix", "Mesa"}));
}

TEST(LRUTest, RemoveVictim){
    LruPolicy p;
    p.on_insert("Norrkoping");
    p.on_insert("Malmo");
    p.on_insert("Mora");
    p.on_remove(p.victim(), EvictionPolicy::RemoveReason::deleted);
    EXPECT_EQ(drain(p,2), (std::vector<std::string>{"Malmo", "Mora"}));
}

TEST(LRUTest, ReinsertionTreatedAsNew){
    LruPolicy p;
    p.on_insert("Krasnoyarsk");
    p.on_insert("Cheboksary");
    p.on_insert("Birobidzhan");
    p.on_remove("Krasnoyarsk", EvictionPolicy::RemoveReason::evicted);
    p.on_insert("Krasnoyarsk");
    EXPECT_EQ(drain(p,3), (std::vector<std::string>{"Cheboksary", "Birobidzhan", "Krasnoyarsk"}));
}

