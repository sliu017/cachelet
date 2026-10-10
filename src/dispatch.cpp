#include "cachelet/dispatch.hpp"
#include "cachelet/resp.hpp"

#include <cctype> // for toupper
#include <chrono>

void to_upper_string(std::string &str){
    for(char &c : str){
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); // commands are case insensitive, keys/values aren't
    }
}

std::string Cachelet::dispatch(Cachelet::Cache &cache, std::vector<std::string> const &command) {
    /*
        Processes the command, a vector of strings, and executes the correct command in the cache accordingly.
    */
    if(command.size() == 0){
        // special case, empty command
        return "";
    }

    std::string command_name = command[0]; // rmbr that command is const
    to_upper_string(command_name);

    bool wrong_arg_count = false;
    if(command_name == "PING"){
        if(command.size() == 1){
            return Cachelet::resp::encode_simple_string("PONG");
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
                return Cachelet::resp::encode_simple_string("OK");
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
                        return Cachelet::resp::encode_error("ERR invalid ttl value, should be an integer");
                    }
                    if(ttl_value <= 0){
                        return Cachelet::resp::encode_error("ERR invalid expire time in 'set' command");
                    }
                } catch (const std::invalid_argument &e){ // not convertible into an int
                    return Cachelet::resp::encode_error("ERR invalid ttl value, should be an integer");
                } catch (const std::out_of_range &e){ // doesnt fit into an int
                    return Cachelet::resp::encode_error("ERR ttl value too large");
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
                    return Cachelet::resp::encode_error("ERR unknown ttl unit '"+ttl_unit +"', try using PX or EX");
                }

                cache.set(key, value, ttl);
                return Cachelet::resp::encode_simple_string("OK");
            }
        } else {
            wrong_arg_count = true;
        }
    } else if(command_name == "GET"){

    } else if(command_name == "DEL"){

    } else if(command_name == "DBSIZE"){
        if(command.size() == 1){
            return Cachelet::resp::encode_integer(cache.size());
        } else {
            wrong_arg_count = true;
        }
    } else if(!wrong_arg_count){ // TODO if commands are added, add more else if blocks. 
        // unknown command
        return Cachelet::resp::encode_error("ERR unknown command '" +command_name +"'");
    }

    if(wrong_arg_count){
        return Cachelet::resp::encode_error("ERR wrong number of arguments");
    }
    return "placeholder"; // eventually have to figure out what happens when we reach here... which in theory shouldn't be possible
}

