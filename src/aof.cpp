#include "cachelet/aof.hpp"
#include "cachelet/resp.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <system_error>

using namespace std::chrono_literals;


namespace {
    void to_lower_string(std::string &str){
        for(char &c : str){
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); 
        }
    }
}

std::optional<Cachelet::AOF::Policy> Cachelet::parse_policy(const std::string &policy_string){
    std::string lower = policy_string;
    to_lower_string(lower);
    if(lower == "always"){
        return Cachelet::AOF::Policy::always;
    } else if(lower == "everysec"){
        return Cachelet::AOF::Policy::everysec;
    } else if(lower == "no"){
        return Cachelet::AOF::Policy::no;
    } else {
        return std::nullopt;
    }
}

// constructor, takes string path and policy
// policy comes in as a cli arg. but conversion should happen where those args are read (catch malformed input there)
Cachelet::AOF::AOF(const std::string &path, Cachelet::AOF::Policy policy)
        : policy(policy),
        last_fsync(std::chrono::steady_clock::now()),
        needs_fsync(false){

    fd = open(path.data(), O_CREAT | O_APPEND | O_WRONLY, 0644); // should be write only?
    // note: 0644 is the permissions in octal, not really too important for single user machine
    
    if(fd == -1){
        throw std::system_error(errno, std::generic_category(),
                                        "failed to open AOF at " +path);
    }
}

Cachelet::AOF::~AOF(){
    close(fd);
}

void Cachelet::AOF::append_command(const std::vector<std::string> &command){
    // Create a RESP array, which follows the format *ELEM_COUNT
    // Then ELEM_COUNT lines of $LENGTH\r\nBYTES\r\n (handled by an already written encode method)
    std::string array = "*" + std::to_string(command.size()) + ("\r\n");
    for(const std::string &token : command){
        array.append(Cachelet::resp::encode_bulk_string(token));
    }
    buffer.append(array);
}

bool Cachelet::AOF::flush(){
    // Architecture: buffer --write()--> page cache --fsync()--> disk
    // always writes contents of the buffer to file, but fsync (because it's relatively slow)
    // frequency is determined by user-set policy
    if(buffer.empty() && !(needs_fsync)){
        // for performance, don't want to run all the code below if no state change in cache
        return true;
    }
    while(!buffer.empty()){
        ssize_t bytes_written = write(fd, buffer.data(), buffer.size());
        if(bytes_written < 0){
            if(errno == EINTR){
                // interrupted, not a fatal issue
                continue;
            } else {
                perror("write");
                return false;
            }
        } else {
            needs_fsync = true;
            buffer.erase(0, bytes_written);
        }
    }

    if(policy == Cachelet::AOF::Policy::always){
        // fsync on every write
        if(fsync(fd) == -1){
            perror("fsync");
            return false;
        }
        needs_fsync = false;
    } else if(policy == Cachelet::AOF::Policy::everysec){
        // fsync if more than a second has passed since the last one
        if(needs_fsync && std::chrono::steady_clock::now() - last_fsync >= 1s){
            if(fsync(fd) == -1){
                perror("fsync");
                return false;
            }
            last_fsync = std::chrono::steady_clock::now();
            needs_fsync = false;
        }
    } else {
        // else: policy is no, so kernel decides when to sync (no fsync implementation required)
        needs_fsync = false;
    }
    
    return true;
}
