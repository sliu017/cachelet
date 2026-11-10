#include <gtest/gtest.h>
#include "cachelet/policies/fifo_policy.hpp"

#include <string>
#include <vector>

using namespace Cachelet;
using Cachelet::FifoPolicy;

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

TEST(FIFOTest, OneKey){
    FifoPolicy p;
    p.on_insert("Amsterdam");
    EXPECT_EQ(p.victim(), "Amsterdam");
}

TEST(FIFOTest, InsertionOrder){
    FifoPolicy p;
    p.on_insert("Lakeland");
    p.on_insert("Tampa");
    p.on_insert("Port St. Lucie");
    EXPECT_EQ(drain(p,3), (std::vector<std::string>{"Lakeland", "Tampa", "Port St. Lucie"}));
}

TEST(FIFOTest, RemoveFromMiddle){
    FifoPolicy p;
    p.on_insert("Phoenix");
    p.on_insert("Tempe");
    p.on_insert("Mesa");
    p.on_remove("Tempe", EvictionPolicy::RemoveReason::deleted);
    EXPECT_EQ(drain(p,2), (std::vector<std::string>{"Phoenix", "Mesa"}));
}

TEST(FIFOTest, RemoveVictim){
    FifoPolicy p;
    p.on_insert("Norrkoping");
    p.on_insert("Malmo");
    p.on_insert("Mora");
    p.on_remove(p.victim(), EvictionPolicy::RemoveReason::deleted);
    EXPECT_EQ(drain(p,2), (std::vector<std::string>{"Malmo", "Mora"}));
}

TEST(FIFOTest, ReinsertionTreatedAsNew){
    FifoPolicy p;
    p.on_insert("Krasnoyarsk");
    p.on_insert("Cheboksary");
    p.on_insert("Birobidzhan");
    p.on_remove("Krasnoyarsk", EvictionPolicy::RemoveReason::evicted);
    p.on_insert("Krasnoyarsk");
    EXPECT_EQ(drain(p,3), (std::vector<std::string>{"Cheboksary", "Birobidzhan", "Krasnoyarsk"}));
}

// Specific to FIFOTest, since on_access does nothing we want to specifically test that
TEST(FIFOTest, AccessDoesNotChangeState){
    FifoPolicy p;
    p.on_insert("Saarbrucken");
    p.on_insert("Patna");
    p.on_insert("Sunshine Coast");
    p.on_access("Patna");
    p.on_access("Saarbrucken");
    p.on_access("Patna");
    EXPECT_EQ(drain(p,3), (std::vector<std::string>{"Saarbrucken", "Patna", "Sunshine Coast"}));
}
