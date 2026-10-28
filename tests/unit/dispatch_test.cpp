#include <gtest/gtest.h>
#include "cachelet/dispatch.hpp"

#include <thread>

using namespace std::chrono_literals;

// ----------------------------------- Case (In)sensitivity -----------------------------------
TEST(DispatchTest, LowercaseCommand){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"set", "Eswatini", "Mbabane"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "+OK\r\n");
    EXPECT_EQ(cache.get("Eswatini"), "Mbabane");
}

TEST(DispatchTest, MixedCaseCommand){
    Cachelet::Cache cache;
    cache.set("Eswatini", "Mbabane");
    std::vector<std::string> cmd = {"GeT", "Eswatini"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "$7\r\nMbabane\r\n");
}

TEST(DispatchTest, LowercaseOption){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"SET", "Eswatini", "Mbabane", "px", "10"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "+OK\r\n");
    EXPECT_EQ(cache.get("Eswatini"), "Mbabane");
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(cache.get("Eswatini"), std::nullopt); // px was honored
}

TEST(DispatchTest, KeysAreCaseSensitive){
    Cachelet::Cache cache;
    std::vector<std::string> set_cmd = {"SET", "Eswatini", "Mbabane"};
    std::vector<std::string> get_cmd = {"GET", "eswatini"};
    Cachelet::dispatch(cache, set_cmd);
    EXPECT_EQ(Cachelet::dispatch(cache, get_cmd), "$-1\r\n"); // different key
}


// ----------------------------------- Ping -----------------------------------

TEST(DispatchTest, Ping){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"PING"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "+PONG\r\n");
}

TEST(DispatchTest, PingTooManyArguments){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"PING", "PONG"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "-ERR wrong number of arguments\r\n");
}

// ----------------------------------- Set -----------------------------------

TEST(DispatchTest, SetKey){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"SET", "Australia", "Canberra"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "+OK\r\n");
    EXPECT_EQ(cache.get("Australia"), "Canberra");
}

TEST(DispatchTest, SetKeyTTL){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"SET", "Australia", "Canberra", "PX", "10"}; // milliseconds
    std::vector<std::string> cmd2 = {"SET", "New Zealand", "Wellington", "EX", "10"}; // seconds
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "+OK\r\n");
    EXPECT_EQ(Cachelet::dispatch(cache, cmd2), "+OK\r\n");

    EXPECT_EQ(cache.get("Australia"), "Canberra");
    EXPECT_EQ(cache.get("New Zealand"), "Wellington");
    std::this_thread::sleep_for(15ms);

    EXPECT_EQ(cache.get("Australia"), std::nullopt); // Should be expired
    EXPECT_EQ(cache.get("New Zealand"), "Wellington"); // Persists
}

TEST(DispatchTest, SetKeyBadTTL){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"SET", "Bhutan", "Thimphu", "EX", "-5"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "-ERR invalid expire time in 'set' command\r\n");
    EXPECT_EQ(cache.size(), 0);
}

TEST(DispatchTest, SetTooFewArguments){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"SET"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "-ERR wrong number of arguments\r\n");
    EXPECT_EQ(cache.size(), 0);
}

TEST(DispatchTest, SetTooManyArguments){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"SET", "South Africa", "Cape Town", "Pretoria"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "-ERR wrong number of arguments\r\n");
    EXPECT_EQ(cache.size(), 0);
}


// ----------------------------------- Get -----------------------------------


TEST(DispatchTest, GetExistingKey){
    Cachelet::Cache cache;
    cache.set("Denmark", "Copenhagen");
    std::vector<std::string> cmd = {"GET", "Denmark"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "$10\r\nCopenhagen\r\n");

}

TEST(DispatchTest, GetNonExistingKey){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"GET", "Denmark"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "$-1\r\n");

}

TEST(DispatchTest, GetTooFewArguments){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"GET"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "-ERR wrong number of arguments\r\n");
}

TEST(DispatchTest, GetTooManyArguments){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"GET", "Denmark", "Sweden"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "-ERR wrong number of arguments\r\n");
}

// ----------------------------------- Del -----------------------------------


TEST(DispatchTest, DelExistingKey){
    Cachelet::Cache cache;
    cache.set("Egypt", "Cairo");
    std::vector<std::string> cmd = {"DEL", "Egypt"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), ":1\r\n");
    EXPECT_EQ(cache.get("Egypt"), std::nullopt);
}

TEST(DispatchTest, DelNonExistingKey){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"DEL", "Egypt"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), ":0\r\n");

}

TEST(DispatchTest, DelTooFewArguments){
    Cachelet::Cache cache;
    cache.set("Egypt", "Cairo");
    std::vector<std::string> cmd = {"DEL"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "-ERR wrong number of arguments\r\n");
    EXPECT_EQ(cache.size(), 1);
}


TEST(DispatchTest, DelTooManyArguments){
    Cachelet::Cache cache;
    cache.set("Egypt", "Cairo");
    cache.set("Libya", "Tripoli");
    std::vector<std::string> cmd = {"DEL", "Egypt", "Libya"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "-ERR wrong number of arguments\r\n");
    EXPECT_EQ(cache.size(), 2);
}


// ----------------------------------- Size -----------------------------------


TEST(DispatchTest, SizeOfEmptyCache){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"DBSIZE"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), ":0\r\n");
}

TEST(DispatchTest, SizeOfPopulatedCache){
    Cachelet::Cache cache;
    cache.set("Bulgaria", "Sofia");
    cache.set("Indonesia", "Jakarta");
    cache.set("East Timor", "Dili");
    std::vector<std::string> cmd = {"DBSIZE"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), ":3\r\n");

}

TEST(DispatchTest, SizeTooManyArguments){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"DBSIZE", "5"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "-ERR wrong number of arguments\r\n");

}

// ----------------------------------- Edge Cases -----------------------------------

TEST(DispatchTest, UnknownCommand){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {"CHICKENNUGGET"};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "-ERR unknown command 'CHICKENNUGGET'\r\n");

}

TEST(DispatchTest, EmptyCommand){
    Cachelet::Cache cache;
    std::vector<std::string> cmd = {};
    EXPECT_EQ(Cachelet::dispatch(cache, cmd), "");
}



