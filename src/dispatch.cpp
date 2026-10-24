#include "cachelet/dispatch.hpp"
#include "cachelet/resp.hpp"
#include "cachelet/aof.hpp"

#include <cctype> // for toupper
#include <chrono>
#include <vector>

namespace {
    void to_upper_string(std::string &str){
        for(char &c : str){
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); // commands are case insensitive, keys/values aren't
        }
    }

    struct DispatchResult {
        std::string reply;
        bool modified; // on commands like SET, DEL etc that change cache state
    };

    DispatchResult dispatch_helper(Cachelet::Cache &cache, const std::vector<std::string> &command){
        /*
            Processes the command, a vector of strings, and executes the correct command in the cache accordingly.
        */
       DispatchResult result = {"", false};
        if(command.size() == 0){
            // special case, empty command
            return result;
        }

        std::string command_name = command[0]; // rmbr that command is const
        to_upper_string(command_name);

        bool wrong_arg_count = false;
        if(command_name == "PING"){
            if(command.size() == 1){
                result.reply = Cachelet::resp::encode_simple_string("PONG");
            } else {
                wrong_arg_count = true;
            }
        } else if(command_name == "SET"){
            // Must be of the form SET KEY VALUE or SET KEY VALUE TTL_UNIT TTL_VALUE
            if(command.size() == 3 || command.size() == 5){
                std::string key = command[1];
                std::string value = command[2];
                if(command.size() == 3){
                    cache.set(key, value);
                    result.modified = true;
                    result.reply = Cachelet::resp::encode_simple_string("OK");
                } else if(command.size() == 5){ // extraneous, but emphasize this is the TTL path
                    
                    std::string ttl_unit = command[3];
                    to_upper_string(ttl_unit);
                    std::string ttl_value_string = command[4];
                    int ttl_value = 0;

                    try {
                        std::size_t parsed_count = 0;
                        ttl_value = std::stoi(ttl_value_string, &parsed_count);
                        if(parsed_count != ttl_value_string.size()){
                            // there was some invalid input following the valid input
                            result.reply = Cachelet::resp::encode_error("ERR invalid ttl value, should be an integer");
                            return result;
                        }
                        if(ttl_value <= 0){
                            result.reply = Cachelet::resp::encode_error("ERR invalid expire time in 'set' command");
                            return result;
                        }
                    } catch (const std::invalid_argument &e){ // not convertible into an int
                        result.reply = Cachelet::resp::encode_error("ERR invalid ttl value, should be an integer");
                        return result;
                    } catch (const std::out_of_range &e){ // doesnt fit into an int
                        result.reply = Cachelet::resp::encode_error("ERR ttl value too large");
                        return result;
                    }

                    std::chrono::steady_clock::duration ttl;
                    if(ttl_unit == "EX"){
                        // Seconds
                        ttl = std::chrono::seconds{ttl_value};
                        
                    } else if(ttl_unit == "PX"){
                        // Milliseconds
                        ttl = std::chrono::milliseconds{ttl_value};
                    } else {
                        // Add support for more units later?
                        result.reply = Cachelet::resp::encode_error("ERR unknown ttl unit '"+ttl_unit +"', try using PX or EX");
                        return result;
                    }

                    cache.set(key, value, ttl);
                    result.modified = true;
                    result.reply = Cachelet::resp::encode_simple_string("OK");
                }
            } else {
                wrong_arg_count = true;
            }
        } else if(command_name == "GET"){
            if(command.size() == 2){
                std::string key = command[1];
                std::optional<std::string> value = cache.get(key);
                if(!value){
                    result.reply = Cachelet::resp::encode_null_bulk_string();
                } else {
                    result.reply = Cachelet::resp::encode_bulk_string(value.value());
                }
            } else {
                wrong_arg_count = true;
            }
        } else if(command_name == "DEL"){
            if(command.size() == 2){
                std::string key = command[1];
                int del_response = cache.del(key) ? 1 : 0;
                result.reply = Cachelet::resp::encode_integer(del_response);
                if(del_response){
                    result.modified = true;
                }
            } else {
                wrong_arg_count = true;
            }
        } else if(command_name == "DBSIZE"){
            if(command.size() == 1){
                result.reply = Cachelet::resp::encode_integer(cache.size());
            } else {
                wrong_arg_count = true;
            }
        } else if(!wrong_arg_count){ // TODO if commands are added, add more else if blocks. 
            // unknown command
            result.reply = Cachelet::resp::encode_error("ERR unknown command '" +command_name +"'");
        }

        if(wrong_arg_count){
            result.reply = Cachelet::resp::encode_error("ERR wrong number of arguments");
        }
        return result;
    }
        
}

std::string Cachelet::dispatch(Cachelet::Cache &cache, const std::vector<std::string> &command) {
    DispatchResult result = dispatch_helper(cache, command);
    return result.reply;
}

std::string Cachelet::dispatch(Cachelet::Cache &cache, AOF& aof, const std::vector<std::string> &command) {
    DispatchResult result = dispatch_helper(cache, command);
    if(result.modified){
        aof.append_command(command);
    }
    return result.reply;
}


