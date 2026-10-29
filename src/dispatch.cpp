#include "cachelet/dispatch.hpp"
#include "cachelet/resp.hpp"
#include "cachelet/aof.hpp"

#include <climits>
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
        std::optional<std::vector<std::vector<std::string>>> log_command; // for aof-specific commands
    };

    // required to correctly introduce eviction to persistence functionality
    // performs a set command, if this set causes eviction of another key, add that eviction to the log in the form of DEL
    void set_with_eviction(Cachelet::Cache &cache, DispatchResult &result, const std::string &key, const std::string &value,
        std::optional<std::chrono::steady_clock::duration> const &ttl){
        std::optional<std::string> evicted_key = std::nullopt;
        if(ttl == std::nullopt){
            evicted_key = cache.set(key, value);
        } else {
            evicted_key = cache.set(key, value, *ttl);
        }
        if(evicted_key != std::nullopt){
            result.log_command.emplace();
            result.log_command->push_back({"DEL", *evicted_key});
        }
    }

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
                    set_with_eviction(cache, result, key, value, std::nullopt);
                    result.modified = true;
                    result.reply = Cachelet::resp::encode_simple_string("OK");
                    if(result.log_command != std::nullopt){ // e.g. eviction occurred, and log_command has been emplaced
                        result.log_command->push_back({"SET", key, value});
                    }

                } else if(command.size() == 5){ // extraneous, but emphasize this is the TTL path
                    
                    std::string ttl_unit = command[3];
                    to_upper_string(ttl_unit);
                    std::string ttl_value_string = command[4];
                    if(ttl_unit == "PX" || ttl_unit == "EX"){
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
                        }

                        set_with_eviction(cache, result, key, value, ttl);
                        result.modified = true;
                        result.reply = Cachelet::resp::encode_simple_string("OK");
                        auto expire_at = std::chrono::system_clock::now() + ttl;
                        auto expire_ms = std::chrono::duration_cast<std::chrono::milliseconds>(expire_at.time_since_epoch()).count();

                        // we need the aof to use PXAT exclusively because it uses system time instead of steady time
                        // (steady time relies on the system being up, which is not guaranteed in the case where AOF would become useful)
                        if(!result.log_command){ // need to ensure result_log exists
                            result.log_command.emplace();
                        }
                        result.log_command->push_back({"SET", key, value, "PXAT", std::to_string(expire_ms)});
                    } else if(ttl_unit == "PXAT" || ttl_unit == "EXAT"){
                        // PXAT, e.g. we set to a specific expiry timestamp
                        // ttl value will be in milliseconds (PXAT) or seconds (EXAT) since epoch
                        long long ttl_value = 0;
                        try {
                            std::size_t parsed_count = 0;
                            ttl_value = std::stoll(ttl_value_string, &parsed_count);
                            if(parsed_count != ttl_value_string.size()){
                                // there was some invalid input following the valid input
                                result.reply = Cachelet::resp::encode_error("ERR invalid ttl value, should be an integer");
                                return result;
                            }
                            if(ttl_value <= 0){
                                result.reply = Cachelet::resp::encode_error("ERR invalid expire time in 'set' command");
                                return result;
                            }
                        } catch (const std::invalid_argument &e){ // not convertible into a long long
                            result.reply = Cachelet::resp::encode_error("ERR invalid ttl value, should be an integer");
                            return result;
                        } catch (const std::out_of_range &e){ // doesnt fit into a long long
                            result.reply = Cachelet::resp::encode_error("ERR ttl value too large");
                            return result;
                        }    
                        // standardize units to milliseconds                    
                        long long expire_ms = ttl_value;
                        if(ttl_unit == "EXAT"){ // EXAT signals the ttl value was passed in as seconds, check conversion
                            if(ttl_value > LLONG_MAX / 1000){
                                result.reply = Cachelet::resp::encode_error("ERR ttl value too large");
                                return result;
                            }
                            expire_ms *= 1000;
                        }
                        // current time in ms since epoch
                        long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>
                            (std::chrono::system_clock::now().time_since_epoch()).count();
                        // time remaining on the ttl
                        long long remaining_ms = expire_ms - now_ms;
                        if(remaining_ms <= 0){
                            // key already expired
                            cache.del(key);
                        } else {
                            // TTL valid, set with the PXAT-derived value
                            set_with_eviction(cache, result, key, value, std::chrono::milliseconds(remaining_ms));
                            if(!result.log_command){ // need to ensure result_log exists
                                result.log_command.emplace();
                            }
                            result.log_command->push_back({"SET", key, value, "PXAT", 
                                std::to_string(std::chrono::milliseconds{expire_ms}.count())});
                        }
                        result.reply = Cachelet::resp::encode_simple_string("OK");
                        result.modified = true;

                    } else {
                        result.reply = Cachelet::resp::encode_error("ERR unknown ttl unit '"+ttl_unit +"', try using PX, EX, PXAT, or EXAT");
                        return result;
                    }

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
        if(result.log_command != std::nullopt){
            for(const std::vector<std::string> &vec : *result.log_command){
                aof.append_command(vec);
            }
        } else {
            aof.append_command(command);
        }
    }
    return result.reply;
}


