#include <gtest/gtest.h>
#include "cachelet/cache.hpp"
#include "cachelet/policies/lru_policy.hpp"

#include <thread>
#include <vector>

using namespace std::chrono_literals;

// To run:
// cmake --build build
// ./build/cache_test

  // (suite of tests, specific test)
TEST(CacheTest, SetAndGetKey){ // set a key and get its value
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p);
    test_cache.set("Lebanon", "Beirut");
    std::optional<std::string> result = test_cache.get("Lebanon");
    EXPECT_EQ(result, "Beirut");
}

TEST(CacheTest, FailToGetKeyThatDoesNotExist){ // we should fail to retrieve a key that is not in the cache
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p);
    test_cache.set("Bangladesh", "Dhaka");
    std::optional<std::string> result = test_cache.get("Jamaica");
    EXPECT_EQ(result, std::nullopt);
}

TEST(CacheTest, GetOverwrittenKey){
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p);
    test_cache.set("Australia", "Sydney");
    test_cache.set("Australia", "Canberra");
    std::optional<std::string> result = test_cache.get("Australia");
    EXPECT_EQ(result, "Canberra");
}

TEST(CacheTest, DeleteKeyThatExists){
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p);
    test_cache.set("Argentina", "Buenos Aires");
    bool result = test_cache.del("Argentina");
    EXPECT_TRUE(result);
}

TEST(CacheTest, DeleteKeyThatDoesNotExist){
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p);
    test_cache.set("Morocco", "Rabat");
    bool result = test_cache.del("Moldova");
    EXPECT_FALSE(result);
}

TEST(CacheTest, GetDeletedKey){
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p); 
    test_cache.set("Ukraine", "Kyiv");
    test_cache.del("Ukraine");
    std::optional<std::string> result = test_cache.get("Ukraine");
    EXPECT_EQ(result, std::nullopt);
}

TEST(CacheTest, SizeOfEmptyCache){
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p);
    std::size_t result = test_cache.size();
    EXPECT_EQ(result, 0);
}

TEST(CacheTest, SizeOfPopulatedCache){
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p);
    test_cache.set("Norway", "Oslo");
    test_cache.set("Maldives", "Male");
    test_cache.set("Malaysia", "Kuala Lumpur");
    std::size_t result = test_cache.size();
    EXPECT_EQ(result, 3);
}

TEST(CacheTest, SizeOfCacheAfterKeyDeletes){
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p);
    test_cache.set("Finland", "Helsinki");
    test_cache.set("Nigeria", "Abuja");
    test_cache.del("Finland");
    std::size_t result = test_cache.size();
    EXPECT_EQ(result, 1);
}

TEST(CacheTest, LRUEvictsCorrectly){
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p, 3);
    test_cache.set("Mexico", "Mexico City");
    test_cache.set("Jamaica", "Kingston");
    test_cache.set("Russia", "Moscow");
    test_cache.set("Turkey", "Ankara");
    std::optional<std::string> result = test_cache.get("Mexico"); // Expect eviction for old value
    EXPECT_EQ(result, std::nullopt);
}

TEST(CacheTest, LRUBringsToFrontOnGet){
     Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p, 2);
    test_cache.set("Netherlands", "Amsterdam");
    test_cache.set("South Korea", "Seoul");
    test_cache.get("Netherlands"); // moves to front
    test_cache.set("Spain", "Madrid");
    std::optional<std::string> result = test_cache.get("Netherlands");
    EXPECT_EQ(result, "Amsterdam"); // shouldn't be null
    result = test_cache.get("South Korea");
    EXPECT_EQ(result, std::nullopt);
}

TEST(CacheTest, LRUBringsToFrontOnSet){
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p, 2);
    test_cache.set("Netherlands", "Maastricht"); // incorrect value at first, to ensure the update is read
    test_cache.set("South Korea", "Seoul");
    test_cache.set("Netherlands", "Amsterdam"); // moves to front
    test_cache.set("Spain", "Madrid");
    std::optional<std::string> result = test_cache.get("Netherlands");
    EXPECT_EQ(result, "Amsterdam"); // shouldn't be null
    result = test_cache.get("South Korea");
    EXPECT_EQ(result, std::nullopt);
}

TEST(CacheTest, KeyWithShortTTLExpires){
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p);
    test_cache.set("France", "Paris", 10ms);
    std::optional<std::string> result = test_cache.get("France");
    EXPECT_EQ(result, "Paris");
    std::this_thread::sleep_for(10ms);
    // Key should have expired
    result = test_cache.get("France");
    EXPECT_EQ(result, std::nullopt);
}

TEST(CacheTest, KeyWithLongTTLPersists){
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p);
    test_cache.set("France", "Paris", 300ms);
    std::optional<std::string> result = test_cache.get("France");
    EXPECT_EQ(result, "Paris");
    std::this_thread::sleep_for(10ms);
    result = test_cache.get("France");
    EXPECT_EQ(result, "Paris");
}

// do we rly need this test
TEST(CacheTest, KeyWithNoTTLPersists){
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p);
    test_cache.set("Canada", "Ottawa");
    std::this_thread::sleep_for(10ms);
    std::optional<std::string> result = test_cache.get("Canada");
    EXPECT_EQ(result, "Ottawa");
}

TEST(CacheTest, SetKeyWithTTLOverridesTTL){
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p);
    test_cache.set("Peru", "Lima", 10ms);

    test_cache.set("Peru", "Lima", 50ms);
    std::optional<std::string> result = test_cache.get("Peru"); 
    EXPECT_EQ(result, "Lima");   

    std::this_thread::sleep_for(15ms);
    // Should still exist, because the TTL was updated to a longer duration.
    result = test_cache.get("Peru");
    EXPECT_EQ(result, "Lima");   
}

TEST(CacheTest, SetKeyWithNoTTLOverridesTTL){
    Cachelet::LruPolicy p;
    Cachelet::Cache test_cache(p);
    test_cache.set("Peru", "Lima", 10ms);

    test_cache.set("Peru", "Lima");
    std::optional<std::string> result = test_cache.get("Peru"); 
    EXPECT_EQ(result, "Lima");   

    std::this_thread::sleep_for(15ms);
    // Should still exist, because the TTL was overriden (now no TTL).
    result = test_cache.get("Peru");
    EXPECT_EQ(result, "Lima");   
}


// Uncomment to check ASan's presence on running the test suite.
// TEST(ASanTest, CheckASanPresence){
//     std::vector<int> vec(5);
//     int result = vec[10]; // should trigger asan
//     EXPECT_EQ(result, 0);
// }

