#include "cachelet/resp.hpp"

#include <iostream>
#include <string>

// such that there is no naming conflict with the method header, but we can use the namespace properties
// while writing the method
namespace Cachelet::resp {

Cachelet::resp::ParseResult parse(const std::string &input){
    // Should return the following in the struct:
    /*
        - some status: Complete, Incomplete, Error
        - std::size_t bytes_consumed
        - std::vector<std::string> command, representing the full command
    */

    ParseResult response; // NOTE: Default status is incomplete, and default bytes consumed is 0!
    std::vector<std::string> command;
   
    // --------- SECTION: Ensure correct formatting to count number of tokens ---------
    // Must start with *X where X is the number of tokens to be processed
    if(input.length() <= 1 || input[0] != '*'){
        // Too short OR does not start with *X!
        // Adjust status accordingly
        if(input.length() >= 1){
            response.status = input[0] == '*' ? Status::Incomplete : Status::Error;
        } else {
            response.status = Status::Incomplete;
        }
        return response; // need to break early
    }
    std::string token_count_string;
    int current_index = 1;
    
    while(current_index < input.size() && input[current_index] != '\r'){
        token_count_string += input[current_index++];
    }
    // Overflowing index here means incomplete, we didn't reach the end we were expecting
    if(current_index >= input.size()){
        return response;
    }

    // --------- SECTION: counts the number of tokens according to the *X value ---------
    int token_count;
    try {
        token_count = std::stoi(token_count_string);
        // Can't have a negative token count!
        if(token_count < 0){
            response.status = Status::Error;
            return response;
        }
    } catch (const std::invalid_argument &e){ // not convertible into an int
        response.status = Status::Error;
        return response; // need to break early
    } catch (const std::out_of_range &e){ // doesnt fit into an int
        response.status = Status::Error;
        return response; // need to break early
    }

    // If empty token count, we're done
    if(token_count == 0){
        response.bytes_consumed = current_index + 2; // for the r, n, noting that current_index is currently at \r 
        response.command = command; 
        response.status = Status::Complete;
        return response;
    }


    // --------- SECTION: move ahead to the first token ---------
    //  ensure it's moving past precisely '\r\n'
    if(input[current_index] != '\r'){
        response.status = Status::Error;
        return response;
    }
    current_index++;
    // Overflowing here means we did not find the first token that was promised to us
    if(current_index >= input.size()){
        return response;
    }
    
    if(input[current_index] != '\n'){
        response.status = Status::Error;
        return response;
    }
    current_index++;
    // Overflowing here means we did not find the first token that was promised to us
    if(current_index >= input.size()){
        return response;
    }




    // --------- SECTION: Parse all tokens according to the count ---------
    int current_token = 0;
    // Could also do away with the current_token variable and write token_count--, but we might need the token count later?
    while(current_index < input.size() && current_token < token_count){
        int this_token_size;

        // --------- SECTION: Read the current token's size, which follows $ ---------
        if(input[current_index] == '$'){
            current_index++;
            std::string token_size_string;
                while(current_index < input.size() && input[current_index] != '\r'){
                    token_size_string += input[current_index++];
                }
                if(current_index >= input.size()){
                    return response;
                }
                try {
                    this_token_size = std::stoi(token_size_string);
                    // Can't have a negative token size!
                    if(this_token_size < 0){
                        response.status = Status::Error;
                        return response;
                    }                    
                } catch (const std::invalid_argument &e){ // not convertible into an int
                    response.status = Status::Error;
                    return response; // need to break early
                } catch (const std::out_of_range &e){ // doesnt fit into an int
                    response.status = Status::Error;
                    return response; // need to break early
                }            
        } else {
            // Not correct format for reading token size, e.g. missing '$'
            response.status = Status::Error;
            return response;
        }

        // read string size of the token size we got
        // first iterate to move past the \r we currently point to, then then \n that should follow it 
        if(input[current_index] != '\r'){
            response.status = Status::Error; 
            return response;
        }
        current_index++;
        if(current_index >= input.size()){ return response; }
        
        if(input[current_index] != '\n'){
            response.status = Status::Error; 
            return response;
        }
        current_index++;
        if(current_index >= input.size()){ return response; }


        // Note this could overflow, which means incomplete
        if(current_index >= input.size()){
            return response;
        }

        // Ensure the token of promised size is in the rest of the input
        // NOTE: must include the +2 to capture the \r\n [2 bytes]
        if(current_index + this_token_size + 2 <= input.size()){
            std::string token = input.substr(current_index, this_token_size);
            command.push_back(token);
        } else {
            // token of promised size not fully present
            return response;
        }

        // Now, move current_index forward and check that it's on the break characters as expected
        current_index += this_token_size;
        // Should land on \r 
        if(input[current_index] != '\r'){
            response.status = Status::Error;
            return response;
        }
        current_index++;
        if(input[current_index] != '\n'){
            response.status = Status::Error;
            return response;
        }
        // OK, good
        current_token++;
        current_index++;

    }
    if(current_token == token_count){
        // read all tokens successfully
        response.command = command;
        response.bytes_consumed = current_index;
        response.status = Status::Complete;
    }

    // if(current_index >= input.size()){
    //     // there were still tokens left that weren't promised
    //     // return incompl? not error i presume
    // }
    return response;

}

}