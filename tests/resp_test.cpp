#include <gtest/gtest.h>
#include "cachelet/resp.hpp"

#include <string>
#include <vector>

using namespace Cachelet::resp; // Note the namespace!

TEST(ParseTest, ParseCorrectlyFormattedSetInput){
    std::string input = "*3\r\n"
                        "$3\r\n"
                        "SET\r\n"
                        "$8\r\n"
                        "Colombia\r\n"
                        "$6\r\n"
                        "Bogota\r\n";
    ParseResult result = parse(input);
    std::vector<std::string> expected = {"SET", "Colombia", "Bogota"};
    EXPECT_EQ(result.command, expected);
    EXPECT_EQ(result.status, Status::Complete);
    EXPECT_EQ(result.bytes_consumed, input.size());
}
// TODO can add tests to see if cache correctly processes the desired command

TEST(ParseTest, ParseCorrectlyFormattedGetInput){
    std::string input = "*2\r\n"
                        "$3\r\n"
                        "GET\r\n"
                        "$7\r\n"
                        "Belarus\r\n";
    ParseResult result = parse(input);
    std::vector<std::string> expected = {"GET", "Belarus"};
    EXPECT_EQ(result.command, expected);
    EXPECT_EQ(result.status, Status::Complete);
    EXPECT_EQ(result.bytes_consumed, input.size());
}

TEST(ParseTest, ParseCorrectlyFormattedDelInput){
    std::string input = "*2\r\n"
                        "$3\r\n"
                        "DEL\r\n"
                        "$8\r\n"
                        "Djibouti\r\n";
    ParseResult result = parse(input);
    std::vector<std::string> expected = {"DEL", "Djibouti"};
    EXPECT_EQ(result.command, expected);
    EXPECT_EQ(result.status, Status::Complete);
    EXPECT_EQ(result.bytes_consumed, input.size());
}

TEST(ParseTest, ParseCorrectlyFormattedSizeInput){
    std::string input = "*1\r\n"
                        "$6\r\n"
                        "DBSIZE\r\n";
    ParseResult result = parse(input);
    std::vector<std::string> expected = {"DBSIZE"}; // ? might need to be rerouted to size() in cache.cpp
    EXPECT_EQ(result.command, expected);
    EXPECT_EQ(result.status, Status::Complete);
    EXPECT_EQ(result.bytes_consumed, input.size());
}
// ---------------------------------------- Incomplete Input ---------------------------------------- //

TEST(ParseTest, ParseEmptyRequest){
    std::string input = "";
    ParseResult result = parse(input);
    EXPECT_EQ(result.status, Status::Incomplete);
}
TEST(ParseTest, ParseIncompleteNumberOfTokens){
    std::string input = "*4\r\n" // should be 3
                        "$3\r\n"
                        "SET\r\n"
                        "$8\r\n"
                        "Colombia\r\n"
                        "$6\r\n"
                        "Bogota\r\n";
    ParseResult result = parse(input);
    EXPECT_EQ(result.status, Status::Incomplete);
}

TEST(ParseTest, ParseIncompleteBreakAtEnd){
    std::string input = "*3\r\n"
                        "$3\r\n"
                        "SET\r\n"
                        "$8\r\n"
                        "Colombia\r\n"
                        "$6\r\n"
                        "Bogota";
    ParseResult result = parse(input);
    EXPECT_EQ(result.status, Status::Incomplete);
}

TEST(ParseTest, ParseIncompleteCommand){
    std::string input = "*1\r\n"
                        "$4\r\n"
                        "PING\r\n"
                        "*2\r\n"
                        "$3\r\n"
                        "GE";

    // Expected Behavior: parse the ping portion completely, then
    // rerun parse on the remaining portion, which should return incomplete
    ParseResult result = parse(input);
    std::vector<std::string> expected = {"PING"};
    EXPECT_EQ(result.command, expected);
    EXPECT_EQ(result.status, Status::Complete);
    EXPECT_EQ(result.bytes_consumed, 14);

    // erase consumed bytes here

    ParseResult second = parse(input.substr(14));
    EXPECT_EQ(second.status, Status::Incomplete);
}


// ---------------------------------------- Malformed Input ---------------------------------------- //


TEST(ParseTest, ParseMalformedInputSizeOfToken){
    std::string input = "*3\r\n"
                        "$3\r\n"
                        "SET\r\n"
                        "$7\r\n" // should be 8
                        "Colombia\r\n"
                        "$6\r\n"
                        "Bogota\r\n";
    ParseResult result = parse(input);
    EXPECT_EQ(result.status, Status::Error);
}

TEST(ParseTest, ParseMalformedInputMissingBreakInMiddle){
    std::string input = "*3\r\n"
                        "$3\r\n"
                        "SET"
                        "$8\r\n"
                        "Colombia\r\n"
                        "$6\r\n"
                        "Bogota\r\n";
    ParseResult result = parse(input);
    EXPECT_EQ(result.status, Status::Error);
}

TEST(ParseTest, ParseMalformedInputJustADollarSymbol){
    std::string input = "$";
    ParseResult result = parse(input);
    EXPECT_EQ(result.status, Status::Error);
}

TEST(ParseTest, ParseMalformedInputTokenCountNotAnInteger){
    std::string input = "*hi\r\n"
                        "$3\r\n"
                        "GET\r\n"
                        "$5\r\n"
                        "Italy\r\n";
    ParseResult result = parse(input);
    EXPECT_EQ(result.status, Status::Error);
}

TEST(ParseTest, ParseMalformedInputIncorrectTokenSizeSymbol){ // e.g, not '$'
    std::string input = "*1\r\n"
                        "#4\r\n"
                        "PING\r\n";
    ParseResult result = parse(input);
    EXPECT_EQ(result.status, Status::Error);
}

TEST(ParseTest, ParseMalformedInputNegativeTokenCount){
    std::string input = "*-3\r\n"
                        "$3\r\n"
                        "SET\r\n"
                        "$8\r\n"
                        "Colombia\r\n"
                        "$6\r\n"
                        "Bogota\r\n";
    ParseResult result = parse(input);
    EXPECT_EQ(result.status, Status::Error);
}

TEST(ParseTest, ParseMalformedInputNegativeTokenSize){
    std::string input = "*3\r\n"
                        "$3\r\n"
                        "SET\r\n"
                        "$8\r\n"
                        "Colombia\r\n"
                        "$-6\r\n"
                        "Bogota\r\n";
    ParseResult result = parse(input);
    EXPECT_EQ(result.status, Status::Error);
}

