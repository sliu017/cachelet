#include "cachelet/dispatch.hpp"


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
            return "+PONG\r\n";
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
                return "+OK\r\n";
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
                        return "-ERR invalid ttl value, should be an integer\r\n";
                    }
                    if(ttl_value <= 0){
                        return "-ERR invalid expire time in 'set' command\r\n";
                    }
                } catch (const std::invalid_argument &e){ // not convertible into an int
                    return "-ERR invalid ttl value, should be an integer\r\n";
                } catch (const std::out_of_range &e){ // doesnt fit into an int
                    return "-ERR ttl value too large\r\n";
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
                    return "-ERR unknown ttl unit '"+ttl_unit +"', try using PX or EX\r\n";
                }

                cache.set(key, value, ttl);
                return "+OK\r\n";
            }
        } else {
            wrong_arg_count = true;
        }
    } else if(command_name == "GET"){

    } else if(command_name == "DEL"){

    } else if(command_name == "DBSIZE"){
        if(command.size() == 1){
            return std::to_string(cache.size());
        } else {
            wrong_arg_count = true;
        }
    } else if(!wrong_arg_count){ // TODO if commands are added, add more else if blocks. 
        // unknown command
        return "-ERR unknown command '" +command_name +"'\r\n";
    }

    if(wrong_arg_count){
        return "-ERR wrong number of arguments\r\n";
    }
    return "placeholder"; // eventually have to figure out what happens when we reach here... which in theory shouldn't be possible
}

