#include <gtest/gtest.h>
#include "simulator.hpp"
#include "cachelet/lru_policy.hpp"

#include <string>
#include <vector>

using namespace Cachelet;

TEST(SimulatorTest, EmptyRequests){
    LruPolicy p;
    sim::SimResult res = sim::simulate({}, 2, p);
    EXPECT_EQ(res.hits, 0u);
    EXPECT_EQ(res.misses, 0u);
}

// the results of this are affected by our choice of LRU!
TEST(SimulatorTest, OneCapacityMiss){
    LruPolicy p;
    sim::SimResult res = sim::simulate({"Markham", "Brampton", "Toronto", "Kingston", "Thunder Bay", "Markham", "Guelph", "Hamilton"},
                                        4, p); // Note Markham is key #1 and key #6 (gets evicted when Thunder Bay enters)
    EXPECT_EQ(res.hits, 0u);
    EXPECT_EQ(res.misses, 8u);
}

// the results of this are affected by our choice of LRU!
TEST(SimulatorTest, ThreeCapacityMisses){
    LruPolicy p;
    sim::SimResult res = sim::simulate({"Daegu", "Busan", "Daejeon", "Pohang", "Daegu", "Sejong", "Busan", "Gwangju", "Incheon", "Pohang"}, 3, p);
    // where the misses are Daegu (state is Pohang->Daejeon->Busan)
    // Busan (state is Sejong->Daegu->Pohang)
    // Pohang (state is Incheon->Gwangju->Busan)
    EXPECT_EQ(res.hits, 0u);
    EXPECT_EQ(res.misses, 10u);
}

TEST(SimulatorTest, AllColdMisses){ // e.g. all keys never seen before so misses by nature
    LruPolicy p;
    sim::SimResult res = sim::simulate({"Ayacucho", "Cusco", "Lima", "Chimbote", "Tacna"}, 10, p);
    EXPECT_EQ(res.hits, 0u);
    EXPECT_EQ(res.misses, 5u);
}

TEST(SimulatorTest, RepeatedKey){ // such that the first is a miss but subsequent reqs are hits
    LruPolicy p;
    sim::SimResult res = sim::simulate({"Walla Walla", "Walla Walla", "Walla Walla"}, 5, p);
    EXPECT_EQ(res.hits, 2u);
    EXPECT_EQ(res.misses, 1u);
}

TEST(SimulatorTest, AlternatingKeysCapacityOne){
    LruPolicy p;
    sim::SimResult res = sim::simulate({"Le Mans", "Le Havre", "Le Mans", "Le Havre"}, 1, p);
    EXPECT_EQ(res.hits, 0u);
    EXPECT_EQ(res.misses, 4u);
}

// results of this test are policy dependent!
TEST(SimulatorTest, PolicyDependent){
    LruPolicy p;
    sim::SimResult res = sim::simulate({"Temuco", "Valdivia", "Temuco", "Arica", "Valdivia"}, 2, p);
    EXPECT_EQ(res.hits, 1u); // Temuco
    EXPECT_EQ(res.misses, 4u);
}

TEST(SimulatorTest, CapacityZero){
    LruPolicy p;
    EXPECT_DEATH(sim::simulate({"Yogyakarta", "Palembang", "Medan", "Pontianak"}, 0, p), "");
}