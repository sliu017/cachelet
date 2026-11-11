#include <gtest/gtest.h>
#include "cachelet/policies/sieve_policy.hpp"

#include <string>
#include <vector>
#include <cstddef>

using namespace Cachelet;
using Cachelet::SievePolicy;

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

TEST(SieveTest, SingleEntry){
    SievePolicy p;
    p.on_insert("a");
    EXPECT_EQ(p.victim(), "a");
    p.on_access("a");
    EXPECT_EQ(drain(p, 1u), (std::vector<std::string>{"a"}));
}

// policy with no accesses should behave identically to FIFO
TEST(SieveTest, NoAccesses){
    SievePolicy p;
    p.on_insert("Haiphong");
    p.on_insert("Adelaide");
    p.on_insert("Puducherry");
    EXPECT_EQ(drain(p, 3u), (std::vector<std::string>{"Haiphong", "Adelaide", "Puducherry"}));
}

TEST(SieveTest, AccessMakesKeyPersistOnce){
    SievePolicy p;
    p.on_insert("a");
    p.on_insert("b");
    p.on_insert("c");
    p.on_access("a");
    EXPECT_EQ(drain(p, 2u), (std::vector<std::string>{"b", "c"})); // b and c get removed before a since a gets its visited bit set
}

TEST(SieveTest, AccessedKeyStillExpires){
    SievePolicy p;
    p.on_insert("a");
    p.on_insert("b");
    p.on_insert("c");
    p.on_access("a");
    EXPECT_EQ(drain(p, 3u), (std::vector<std::string>{"b", "c", "a"})); 
}

TEST(SieveTest, HandWrapsAround){
    SievePolicy p;
    p.on_insert("a");
    p.on_insert("b");
    p.on_insert("c");
    p.on_access("a");
    p.on_access("b");
    p.on_access("c");
    //  c  ->  b  ->  a
    // vis    vis    vis
    EXPECT_EQ(drain(p, 3u), (std::vector<std::string>{"a", "b", "c"}));
}

TEST(SieveTest, RemoveHandInMiddle){
    SievePolicy p;
    p.on_insert("a");
    p.on_insert("b");
    p.on_insert("c");
    p.on_access("a");
    p.victim(); // forces the hand towards b
    p.on_remove("b", EvictionPolicy::RemoveReason::deleted);
    EXPECT_EQ(p.victim(), "c"); // should move in the "forward" direction to land on c  
}

TEST(SieveTest, RemoveHandAtEnd){
    SievePolicy p;
    p.on_insert("a");
    p.on_insert("b");
    p.on_insert("c");
    p.on_access("a");
    p.on_access("b");
    p.victim(); // forces the hand towards c
    p.on_remove("c", EvictionPolicy::RemoveReason::deleted);
    EXPECT_EQ(p.victim(), "a"); // should wrap around in the "forward" direction to land on a    
}

TEST(SieveTest, NewKeyInsertedAheadOfHand){
    SievePolicy p;
    p.on_insert("a");
    p.on_insert("b");
    p.on_insert("c");
    p.on_access("a");
    p.victim(); // forces the hand towards b
    p.on_insert("d");
    EXPECT_EQ(drain(p, 4u), (std::vector<std::string>{"b", "c", "d", "a"}));
}
