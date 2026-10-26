#include <gtest/gtest.h>
#include "cachelet/aof.hpp"
#include "cachelet/cache.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace {
    std::filesystem::path write_temp_aof(const std::string& contents){
        auto path = std::filesystem::temp_directory_path() / "cachelet_test.aof";
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << contents;
        return path;
    }
}

TEST(ReplayTest, CleanLogRestoresState){
    std::string log = 
        "*3\r\n$3\r\nSET\r\n$4\r\nIraq\r\n$7\r\nBaghdad\r\n"   // SET Iraq Baghdad
        "*3\r\n$3\r\nSET\r\n$6\r\nBhutan\r\n$7\r\nThimphu\r\n"   // SET Bhutan Thimphu
        "*2\r\n$3\r\nDEL\r\n$4\r\nIraq\r\n";             // DEL Iraq
    auto path = write_temp_aof(log);

    Cachelet::Cache cache;
    Cachelet::replay(path.string(), cache);

    EXPECT_EQ(cache.get("Iraq"), std::nullopt);
    EXPECT_EQ(cache.get("Bhutan"), "Thimphu");
    EXPECT_EQ(cache.size(), 1u);
    std::filesystem::remove(path);
}

TEST(ReplayTest, ReplayWithNoFile){
    auto path = std::filesystem::temp_directory_path() / "cachelet_test.aof";
    std::filesystem::remove(path);   // remove the file
    Cachelet::Cache cache;
    EXPECT_NO_THROW(Cachelet::replay(path.string(), cache)); // no file found, but shouldn't throw
    EXPECT_EQ(cache.size(), 0u);
}