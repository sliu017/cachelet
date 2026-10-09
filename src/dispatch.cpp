#include "cachelet/dispatch.hpp"

#include <cctype> // for toupper

std::string Cachelet::dispatch(Cachelet::Cache &cache, std::vector<std::string> const &command) {
    /*
        Processes the command, a vector of strings, and executes the correct command in the cache accordingly.
    */
    if(command.size() == 0){
        // special case, empty command
        return "";
    }

    std::string command_name = command[0];
    for(char &c : command_name){
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); // commands are case insensitive, keys/values aren't
    }

    bool wrong_arg_count = false;
    if(command_name == "PING"){
        if(command.size() == 1){
            return "+PONG\r\n";
        } else {
            wrong_arg_count = true;
        }
    } else if(command_name == "SET"){

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