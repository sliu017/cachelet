#include "cachelet/aof.hpp"
#include "cachelet/resp.hpp"
#include "cachelet/cache.hpp"
#include "cachelet/dispatch.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <cerrno>
#include <cstring>
#include <cctype>
#include <system_error>
#include <stdexcept>

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

// Reconstruct the cache using the AOF (which is essentially a log of state-changing commands)
void Cachelet::replay(const std::string &path, Cachelet::Cache &cache){
    int fd = open(path.data(), O_RDWR, 0644); // idk if O_CREAT is needed cause we don't want to create then read from an empty file
    if(fd == -1){
        if(errno == ENOENT){
            // file doesnt exist which is expected on startup
            return;
        }
        throw(std::system_error(errno, std::generic_category(), "failed to open AOF at " +path));
    }
    std::string contents;
    char chunk[65536];
    int replayed_command_count = 0;
    // read command string from aof file
    while(true){
        ssize_t bytes_read = read(fd, chunk, sizeof(chunk));
        if(bytes_read == 0){
            break;
        }
        if(bytes_read == -1){
            if(errno == EINTR){
                continue;
            }
            int read_errno = errno; // close might overwrite errno
            close(fd);
            throw std::system_error(read_errno, 
                std::generic_category(), "failed to read AOF at " + path);

        }
        contents.append(chunk, bytes_read); // without bytes_read argument, we might append extraneous chars in chunk
    }
    size_t original_size = contents.size();
    size_t valid_bytes = 0; // we might have some invalid bytes in the file, so we should clean it up here
    while(true){
        if(contents.empty()){
            break;
        }
        Cachelet::resp::ParseResult result = Cachelet::resp::parse(contents);
        
        if(result.status == Cachelet::resp::Status::Complete){
            dispatch(cache, result.command);
            replayed_command_count++;
            valid_bytes += result.bytes_consumed;
            contents.erase(0, result.bytes_consumed); // TODO time sink, optimize later
            continue;
        } else if(result.status == Cachelet::resp::Status::Incomplete){
            // stop at the first incomplete command and remove it so it doesnt affect later commands
            // (we can assume incomplete here means something crashed mid-write, so the client never got a reply)
            // ^^ therefore dropping it entirely is safe/consistent
            if(ftruncate(fd, valid_bytes) == -1){
                int truncate_errno = errno; // close might overwrite errno
                close(fd);
                throw std::system_error(truncate_errno, std::generic_category(), "aof: failed to ftruncate to " +std::to_string(valid_bytes)
                 +" bytes");
            }
            std::cerr << "AOF: Dropped incomplete command with " << (original_size - valid_bytes) << " bytes " << '\n';
            break;
        } else if(result.status == Cachelet::resp::Status::Error){
            // theres something wrong here since all of contents should have been encoded by our own functions
            close(fd);
            throw std::runtime_error("AOF corrupt at byte offset " + std::to_string(valid_bytes) +", refusing to start");
        }
    }
    close(fd);
    std::cout << "Replayed " << replayed_command_count << " commands successfully\n";
}
