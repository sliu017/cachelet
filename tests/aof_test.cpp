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

TEST(ReplayTest, ReplayWithEmptyAOFFile){
    std::string log = ""; // empty AOF
    auto path = write_temp_aof(log);
    Cachelet::Cache cache;
    EXPECT_NO_THROW(Cachelet::replay(path.string(), cache));
    EXPECT_EQ(cache.size(), 0u);
}

TEST(ReplayTest, IncompleteLog){
    std::string valid = 
        "*3\r\n$3\r\nSET\r\n$4\r\nIraq\r\n$7\r\nBaghdad\r\n"   // SET Iraq Baghdad
        "*3\r\n$3\r\nSET\r\n$6\r\nBhutan\r\n$7\r\nThimphu\r\n";   // SET Bhutan Thimphu
    std::string partial = "*2\r\n$3\r\nDEL\r\n$4\r";         // cutoff
    std::string log = valid + partial;
    auto path = write_temp_aof(log);
    Cachelet::Cache cache;
    EXPECT_NO_THROW(Cachelet::replay(path.string(), cache));
    EXPECT_EQ(cache.size(), 2u);
    EXPECT_EQ(cache.get("Iraq"), "Baghdad");
    EXPECT_EQ(cache.get("Bhutan"), "Thimphu");
    EXPECT_EQ(std::filesystem::file_size(path), valid.size()); // Note that replay truncates the FILE on disk, not the in memory string
}

TEST(ReplayTest, CorruptedLog){
    std::string log = 
        "*3\r\n$3\r\nSET\r\n$4\r\nIraq\r\n$7\r\nBaghdad\r\n"   // SET Iraq Baghdad
        "*3\r\n$3\r\nSET\r\n$6\r\nBhutan\r\n$7\r\nThimphu\r\n"   // SET Bhutan Thimphu
        "*3\r\n$5\r\nERROR\r\n$6\r\nBANANA\r\n" // Erroneous line, wrong token count + invalid commands
        "*2\r\n$3\r\nDEL\r\n$4\r\nIraq\r\n";        
    auto path = write_temp_aof(log);
    Cachelet::Cache cache;
    EXPECT_ANY_THROW(Cachelet::replay(path.string(), cache));
}