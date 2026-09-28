#include <gtest/gtest.h>
#include "cachelet/cache.hpp"

#include <vector>

// To run:
// cmake --build build
// ./build/cache_test

  // (suite of tests, specific test)
TEST(CacheTest, SetAndGetKey){ // set a key and get its value
    Cachelet::Cache test_cache;
    test_cache.set("Lebanon", "Beirut");
    std::optional<std::string> result = test_cache.get("Lebanon");
    EXPECT_EQ(result, "Beirut");
}

TEST(CacheTest, FailToGetKeyThatDoesNotExist){ // we should fail to retrieve a key that is not in the cache
    Cachelet::Cache test_cache;
    test_cache.set("Bangladesh", "Dhaka");
    std::optional<std::string> result = test_cache.get("Jamaica");
    EXPECT_EQ(result, std::nullopt);
}

TEST(CacheTest, GetOverwrittenKey){
    Cachelet::Cache test_cache;
    test_cache.set("Australia", "Sydney");
    test_cache.set("Australia", "Canberra");
    std::optional<std::string> result = test_cache.get("Australia");
    EXPECT_EQ(result, "Canberra");
}

TEST(CacheTest, DeleteKeyThatExists){
    Cachelet::Cache test_cache;
    test_cache.set("Argentina", "Buenos Aires");
    bool result = test_cache.del("Argentina");
    EXPECT_TRUE(result);
}

TEST(CacheTest, DeleteKeyThatDoesNotExist){
    Cachelet::Cache test_cache;
    test_cache.set("Morocco", "Rabat");
    bool result = test_cache.del("Moldova");
    EXPECT_FALSE(result);
}

TEST(CacheTest, GetDeletedKey){
    Cachelet::Cache test_cache;
    test_cache.set("Ukraine", "Kyiv");
    test_cache.del("Ukraine");
    std::optional<std::string> result = test_cache.get("Ukraine");
    EXPECT_EQ(result, std::nullopt);
}

TEST(CacheTest, SizeOfEmptyCache){
    Cachelet::Cache test_cache;
    std::size_t result = test_cache.size();
    EXPECT_EQ(result, 0);
}

TEST(CacheTest, SizeOfPopulatedCache){
    Cachelet::Cache test_cache;
    test_cache.set("Norway", "Oslo");
    test_cache.set("Maldives", "Male");
    test_cache.set("Malaysia", "Kuala Lumpur");
    std::size_t result = test_cache.size();
    EXPECT_EQ(result, 3);
}

TEST(CacheTest, SizeOfCacheAfterKeyDeletes){
    Cachelet::Cache test_cache;
    test_cache.set("Finland", "Helsinki");
    test_cache.set("Nigeria", "Abuja");
    test_cache.del("Finland");
    std::size_t result = test_cache.size();
    EXPECT_EQ(result, 1);
}

TEST(CacheTest, LRUEvictsCorrectly){
    Cachelet::Cache test_cache(3);
    test_cache.set("Mexico", "Mexico City");
    test_cache.set("Jamaica", "Kingston");
    test_cache.set("Russia", "Moscow");
    test_cache.set("Turkey", "Ankara");
    std::optional<std::string> result = test_cache.get("Mexico"); // Expect eviction for old value
    EXPECT_EQ(result, std::nullopt);
}

TEST(CacheTest, LRUBringsToFrontOnGet){
    Cachelet::Cache test_cache(2);
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
    Cachelet::Cache test_cache(2);
    test_cache.set("Netherlands", "Maastricht"); // incorrect value at first, to ensure the update is read
    test_cache.set("South Korea", "Seoul");
    test_cache.set("Netherlands", "Amsterdam"); // moves to front
    test_cache.set("Spain", "Madrid");
    std::optional<std::string> result = test_cache.get("Netherlands");
    EXPECT_EQ(result, "Amsterdam"); // shouldn't be null
    result = test_cache.get("South Korea");
    EXPECT_EQ(result, std::nullopt);
}

// Uncomment to check ASan's presence on running the test suite.
// TEST(ASanTest, CheckASanPresence){
//     std::vector<int> vec(5);
//     int result = vec[10]; // should trigger asan
//     EXPECT_EQ(result, 0);
// }
