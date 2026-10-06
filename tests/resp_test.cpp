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

TEST(ParseTest, ParseValueContainingCRLF){
    // parse a value containing '\r\n' (kind of a contrived example but i like my capitals theme lol)
    std::string input = "*3\r\n"
                        "$3\r\n"
                        "SET\r\n"
                        "$8\r\n"
                        "Ethiopia\r\n"
                        "$12\r\n"
                        "Addis\r\nAbaba\r\n";
    ParseResult result = parse(input);
    std::vector<std::string> expected = {"SET", "Ethiopia", "Addis\r\nAbaba"};
    EXPECT_EQ(result.status, Status::Complete);
    EXPECT_EQ(result.command, expected);
    EXPECT_EQ(result.bytes_consumed, input.size());
}

TEST(ParseTest, ParseEmptyValue){
    // Fun fact: Nauru has no formally designated capital!
    std::string input = "*3\r\n"
                        "$3\r\n"
                        "SET\r\n"
                        "$5\r\n"
                        "Nauru\r\n"
                        "$0\r\n"
                        "\r\n";
    ParseResult result = parse(input);
    std::vector<std::string> expected = {"SET", "Nauru", ""};
    EXPECT_EQ(result.status, Status::Complete);
    EXPECT_EQ(result.command, expected);
    EXPECT_EQ(result.bytes_consumed, input.size());
}

// When given two (or more) complete commands, the parser's job is only to complete the first complete one & leave the rest in the buffer
TEST(ParseTest, ParseTwoCompleteCommands){
    std::string first = "*1\r\n"
                        "$4\r\n"
                        "PING\r\n";
    std::string second = "*2\r\n"
                        "$3\r\n"
                        "GET\r\n"
                        "$7\r\n"
                        "Belarus\r\n";
    ParseResult result = parse(first + second);
    std::vector<std::string> expected = {"PING"};
    EXPECT_EQ(result.status, Status::Complete);
    EXPECT_EQ(result.command, expected);
    EXPECT_EQ(result.bytes_consumed, first.size());
}

TEST(ParseTest, ParseEmptyCommand){
    std::string input = "*0\r\n";
    ParseResult result = parse(input);
    std::vector<std::string> expected = {};
    EXPECT_EQ(result.status, Status::Complete);
    EXPECT_EQ(result.command, expected);
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

    // the server erases consumed bytes here

    ParseResult second = parse(input.substr(14));
    EXPECT_EQ(second.status, Status::Incomplete);
}

// Try every single "cutoff" point for errors
TEST(ParseTest, ParseEveryPrefixIsIncomplete){
    std::string input = "*3\r\n"
                        "$3\r\n"
                        "SET\r\n"
                        "$10\r\n"
                        "Uzbekistan\r\n"
                        "$8\r\n"
                        "Tashkent\r\n";
    for(std::size_t i = 0; i < input.size(); i++){
        ParseResult result = parse(input.substr(0, i));
        EXPECT_EQ(result.status, Status::Incomplete) << "failed at prefix length " << i;
    }
}

TEST(ParseTest, ParseEmptyArrayMissingNewline){
    // where \r is present but not \n
    std::string input = "*0\r";
    ParseResult result = parse(input);
    EXPECT_EQ(result.status, Status::Incomplete);
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

TEST(ParseTest, ParseMalformedInputGarbageAfterTokenCount){
    std::string input = "*3abc\r\n" // Unwanted characters after 3
                        "$3\r\n"
                        "SET\r\n"
                        "$8\r\n"
                        "Colombia\r\n"
                        "$6\r\n"
                        "Bogota\r\n";
    ParseResult result = parse(input);
    EXPECT_EQ(result.status, Status::Error);
}

TEST(ParseTest, ParseMalformedInputTokenSizeTooLarge){
    std::string input = "*1\r\n"
                        "$99999999999\r\n" // too large!
                        "Liechtenstein\r\n";
    ParseResult result = parse(input);
    EXPECT_EQ(result.status, Status::Error);
}

// ---------------------------------------- Encode Test ----------------------------------------
TEST(EncodeTest, EncodeSimpleString){
    EXPECT_EQ(encode_simple_string("OK"), "+OK\r\n");
}

TEST(EncodeTest, EncodeEmptySimpleString){
    EXPECT_EQ(encode_simple_string(""), "+\r\n");
}

TEST(EncodeTest, EncodeError){
    EXPECT_EQ(encode_error("ERROR: Invalid command."), "-ERROR: Invalid command.\r\n");
}

TEST(EncodeTest, EncodeEmptyError){
    EXPECT_EQ(encode_error(""), "-\r\n");
}

TEST(EncodeTest, EncodeSmallInteger){
    EXPECT_EQ(encode_integer(9), ":9\r\n");
}

TEST(EncodeTest, EncodeLargeInteger){
    EXPECT_EQ(encode_integer(999999999), ":999999999\r\n");
}

// 15 9s
TEST(EncodeTest, EncodeLarge64BitInteger){
    EXPECT_EQ(encode_integer(999999999999999), ":999999999999999\r\n");
}

TEST(EncodeTest, EncodeNegativeInteger){
    EXPECT_EQ(encode_integer(-720), ":-720\r\n");
}

TEST(EncodeTest, EncodeSmallBulkString){
    EXPECT_EQ(encode_bulk_string("Lisbon"), "$6\r\nLisbon\r\n");
}

TEST(EncodeTest, EncodeBulkStringWithSpaces){
    EXPECT_EQ(encode_bulk_string("Federated States of Micronesia"), 
    "$30\r\nFederated States of Micronesia\r\n");
}

TEST(EncodeTest, EncodeEmptyBulkString){
    EXPECT_EQ(encode_bulk_string(""), "$0\r\n");
}

TEST(EncodeTest, EncodeNullBulkString){
    EXPECT_EQ(encode_null_bulk_string(), "$-1\r\n");
}
