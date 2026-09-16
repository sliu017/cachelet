#include <gtest/gtest.h>
#include "cachelet/cache.hpp"

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
