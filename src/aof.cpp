#include "cachelet/aof.hpp"
#include "cachelet/resp.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <cerrno>
#include <cstring>

// constructor, takes string path and policy
// policy comes in as a cli arg. but conversion should happen where those args are read (catch malformed input there)
Cachelet::AOF::AOF(const std::string &path, Cachelet::AOF::Policy policy)
        : policy(policy),
        last_fsync(std::chrono::steady_clock::now()){

    fd = open(path.data(), O_CREAT | O_APPEND | O_WRONLY, 0644); // should be write only?
    // note: 0644 is the permissions in octal, not really too important for single user machine
    
    if(fd == -1){
        throw std::system_error(errno, std::generic_category(),
                                        "failed to open AOF at " +path);
    }
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

}