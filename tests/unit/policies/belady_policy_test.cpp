
#include <gtest/gtest.h>
#include "cachelet/policies/belady_policy.hpp"
#include "simulator.hpp"

#include <limits>
#include <string>
#include <vector>

namespace {

constexpr std::size_t INF = std::numeric_limits<std::size_t>::max();

// Test the compute_next_access_times method correctly takes a string of vectors and returns each one's
// next time (e.g. index) of access.

// for an empty string of vectors, return an empty string of access times
TEST(NextAccessTest, EmptyTrace) {
    EXPECT_TRUE(Cachelet::sim::compute_next_access_times({}).empty());
}


TEST(NextAccessTest, MixedKeys) {
    // a b a c a
    // 2 I 4 I I (I = INF)
    std::vector<std::string> reqs{"a", "b", "a", "c", "a"};
    std::vector<std::size_t> expected{2, INF, 4, INF, INF};
    EXPECT_EQ(Cachelet::sim::compute_next_access_times(reqs), expected);
}

TEST(NextAccessTest, RepeatedKey) {
    std::vector<std::string> reqs{"x", "x", "x"};
    std::vector<std::size_t> expected{1, 2, INF};
    EXPECT_EQ(Cachelet::sim::compute_next_access_times(reqs), expected);
}

// Tests for the Belady policy behavior

TEST(BeladyTest, EvictsFurthestNotLeastRecent) {
    // ensure Belady looks ahead to know it should evict b, not a
    // despite a being the least recently used at that point
    std::vector<std::string> reqs{"a", "b", "a"};
    Cachelet::BeladyPolicy policy(Cachelet::sim::compute_next_access_times(reqs));
    policy.on_insert("a");
    policy.on_insert("b");
    EXPECT_EQ(policy.victim(), "b");
}

// when we call get(key), the next_access on key moves forward
TEST(BeladyTest, GetUpdatesNextAccess) {
    std::vector<std::string> reqs{"a", "b", "a", "b", "a"};
    Cachelet::BeladyPolicy policy(Cachelet::sim::compute_next_access_times(reqs));
    policy.on_insert("a"); 
    policy.on_insert("b");
    policy.on_access("a"); // note that a's next usage is 4, while b's is 3, so a should currently be the victim selection
    EXPECT_EQ(policy.victim(), "a");
}

// for a b c a, b and c both have next accesses of INF, however we must ensure they don't collapse into the same pair
// the above bug could be caused by a custom comparator that doesn't check the second value, as i'd mistakenly implemented previously
TEST(BeladyTest, TiedNextAccessesAreKept) {
    std::vector<std::string> reqs{"a", "b", "c", "a"};
    Cachelet::BeladyPolicy policy(Cachelet::sim::compute_next_access_times(reqs));
    policy.on_insert("a");
    policy.on_insert("b");
    policy.on_insert("c");

    std::string first_victim = policy.victim();
    EXPECT_TRUE(first_victim == "b" || first_victim == "c");
    policy.on_remove(first_victim, Cachelet::EvictionPolicy::RemoveReason::evicted);

    std::string second_victim = policy.victim();
    EXPECT_TRUE(second_victim == "b" || second_victim == "c");
    EXPECT_NE(first_victim, second_victim); // both removes should be unique
    policy.on_remove(second_victim, Cachelet::EvictionPolicy::RemoveReason::evicted);

    EXPECT_EQ(policy.victim(), "a");
}


TEST(BeladyTest, HandleRemovedKey) {
    std::vector<std::string> reqs{"a", "b", "a"};
    Cachelet::BeladyPolicy policy(Cachelet::sim::compute_next_access_times(reqs));
    policy.on_insert("a");
    policy.on_insert("b");
    policy.on_remove("b", Cachelet::EvictionPolicy::RemoveReason::deleted);
    EXPECT_EQ(policy.victim(), "a");
}

// end to end tests

// capacity = 3, optimal behavior:
// reqs: 1 2 3 4  1 2 5  1 2 3 4 5
// next: 4 5 9 10 7 8 11 I I I I I
// vic:  / / / 3  H H 4  H H 3 2 H TOTAL: 5 hits, 7 misses
//                           [note: 3 evicted before 1, despite both having next accesses of INF, 
//                           b/c the pairs are sorted secondarily on keys ascending, so rbegin() gets 3 over 1]
TEST(BeladySimTest, SimulateEndToEnd) {
    std::vector<std::string> reqs{"1", "2", "3", "4", "1", "2", "5", "1", "2", "3", "4", "5"};
    Cachelet::BeladyPolicy policy(Cachelet::sim::compute_next_access_times(reqs));
    auto result = Cachelet::sim::simulate(reqs, 3, policy);
    EXPECT_EQ(result.hits, 5u);
    EXPECT_EQ(result.misses, 7u);
}

// capacity = 2:
// reqs: 1 2 3 1 2
// vic:  / / 2 H / TOTAL: 1 hit, 4 misses [LRU gets 0 hits]
// meant to test that Belady evicts 2 instead of 1, despite 1 being LRU, because 2 is the key used furthest in the future
TEST(BeladySimTest, BeatsLRU) {
    std::vector<std::string> reqs{"1", "2", "3", "1", "2"};
    Cachelet::BeladyPolicy policy(Cachelet::sim::compute_next_access_times(reqs));
    auto result = Cachelet::sim::simulate(reqs, 2, policy);
    EXPECT_EQ(result.hits, 1u);
    EXPECT_EQ(result.misses, 4u);
}

// shouldnt be any misses other than insertion misses when we have the capacity for everything (5, 3 unique keys)
TEST(BeladySimTest, EnoughCapacityForAllKeys) {
    std::vector<std::string> reqs{"1", "2", "3", "1", "2"};
    Cachelet::BeladyPolicy policy(Cachelet::sim::compute_next_access_times(reqs));
    auto result = Cachelet::sim::simulate(reqs, 5, policy);
    EXPECT_EQ(result.hits, 2u);
    EXPECT_EQ(result.misses, 3u);
}

}