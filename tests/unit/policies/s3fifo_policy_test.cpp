#include <gtest/gtest.h>
#include "cachelet/policies/s3fifo_policy.hpp"

using namespace Cachelet;
using Cachelet::S3FifoPolicy;
#include <unordered_set>
#include <optional>

// Runs before every test, sets up the req() simulator
class S3FifoTest : public ::testing::Test {
    protected:
        static constexpr std::size_t capacity = 10;
        S3FifoPolicy policy{capacity};
        std::unordered_set<std::string> resident;
        
        // drain doesn't really work here since we expect evictions to be followed by insertions (for the ghost queue logic to work)
        // therefore we write a more realistic req() method to simulate cache activity
        std::optional<std::string> req(const std::string &key){
            if(resident.contains(key)){
                policy.on_access(key);
                return std::nullopt;
            }
            std::optional<std::string> evicted;
            if(resident.size() >= capacity){
                evicted = policy.victim();
                resident.erase(*evicted);
                policy.on_remove(*evicted, EvictionPolicy::RemoveReason::evicted);
            }
            resident.insert(key);
            policy.on_insert(key);
            return evicted;
        }
};

TEST_F(S3FifoTest, NoHitsBehavesLikeFIFO){
    req("Astrakhan");
    req("Berlin");
    req("Carlsbad");
    req("Durango");
    req("Erdenet");
    req("Fuzhou");
    req("Guanambi");
    req("Huntsville");
    req("Iquitos");
    req("Jeddah"); // 10
    EXPECT_EQ(req("Kagoshima"), "Astrakhan");
    EXPECT_EQ(req("Liverpool"), "Berlin");
}

TEST_F(S3FifoTest, SGetsPromotedToM){
    std::vector<std::string> keys{"a","1","2","3","4","5","6","7","8","9"}; // full cache
    for(const std::string &s : keys){
        req(s);
    }
    req("a");
    req("a"); // freq >= 2, so up for promotion
    EXPECT_EQ(req("dummy"), "1"); // a should get promoted to M and 1 should be evicted
    EXPECT_EQ(req("another dummy"), "2"); // a persists
    EXPECT_EQ(req("third dummy"), "3");
}

TEST_F(S3FifoTest, EvictionInM){
    std::vector<std::string> keys{"0","1","2","3","4","5","6","7","8","9"};
    for(const std::string &s : keys){
        req(s);
        req(s);
        req(s); // all are freq >= 2 so all will be promoted to M on next evict call
    }
    EXPECT_EQ(req("dummy"), "0"); // goes back to FIFO behavior as all have freq = 0 upon promotion
    req("dummy");
    req("dummy"); // dummy now has freq >= 2 so it will be promoted and sent to the HEAD of the doubly linked list (newest/safest)
    req("1");
    req("2");
    EXPECT_EQ(req("new dummy"), "3"); // cycles to 3 since 1 and 2 now have a bumped freq
    EXPECT_EQ(req("newer dummy"), "new dummy"); // still removes from S
}

TEST_F(S3FifoTest, GhostHitReadmitsToMain){
    std::vector<std::string> keys{"1","2","3","4","5","6","7","8","9"};
    for(const std::string &s : keys){
        req(s);
        req(s);
        req(s); // all numeric keys up for promotion
    }
    req("spooky");
    EXPECT_EQ(req("dummy"), "spooky"); // spooky gets moved to ghost queue, 1-9 are moved to main w/0 freq 0
    req("dummy");
    req("dummy"); // dummy is up for promotion
    // cache now contains dummy at S (ready for promo) and numeric keys 9->8...->1 (1 first evicted), all freq 0
    EXPECT_EQ(req("spooky"), "1"); // spooky gets promoted to main from ghost, where 1 is evicted
    EXPECT_EQ(req("mundane"), "2"); // spooky persists in main (if it was in S it would be evicted)
}

TEST_F(S3FifoTest, GhostKeyGetsEvictedAtCapacity){
    std::vector<std::string> keys{"0","1","2","3","4","5","6","7","8","9"};
    for(const std::string &s : keys){
        req(s); // all remain in S, not getting promoted
    }
    for(const std::string &s : keys){
        req(s + "new"); // meant to just flush out all the numeric keys into ghost
    }
    // at this point 1 - 9 will be in the ghost queue (with capacity 9), but 0 will have been evicted from it
    // S contains 0new through 9new
    req("0"); // 0 goes into S, pushing out 0new (now: 1new -> ... 9new -> 0)

    for(const std::string &s : keys){
        if(s != "0"){
            req(s + "newer"); // 0 now should sit at the tail of S, ready for eviction [if it was sent to S and not M]
        }
    }
    EXPECT_EQ(req("fun"), "0"); // 0 will be evicted if it was truly sent to S the first time and not M
}

// deleting from the middle of the cache shouldn't sent the key to ghost queue
TEST_F(S3FifoTest, DelCreatesNoGhost){
    std::vector<std::string> keys{"0","1","2","3","4","5","6","7","8","9"};
    for(const std::string &s : keys){
        req(s);
        req(s);
        req(s); // all 0-9 are up for promotion
    }
    policy.on_remove("3", EvictionPolicy::RemoveReason::deleted); // not bc evicted
    resident.erase("3");
    req("dummy"); // now cache is full again 
    req("dummier"); // causes numeric keys to all be promoted to M, dummy evicted, S contains dummier
    req("3"); // if our del created a ghost, 3 would go straight to main. otherwise, it's in S. [dummier evicted]
    EXPECT_EQ(req("riposte"), "3"); // expect 3 to be evicted from S (otherwise 0 evicted)
}
