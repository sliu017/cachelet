#include "cachelet/aof.hpp"
#include "cachelet/resp.hpp"
#include "cachelet/cache.hpp"
#include "cachelet/dispatch.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <filesystem>
#include <iostream>
#include <cerrno>
#include <cctype>
#include <system_error>
#include <stdexcept>
#include <sys/types.h>
#include <sys/stat.h>


using namespace std::chrono_literals;


namespace {
    void to_lower_string(std::string &str){
        for(char &c : str){
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); 
        }
    }

    // continuously calls write() on the remaining bytes until everything is written
    // only returns false on a fatal error
    bool write_all(int fd, const std::string &str){
        size_t offset = 0;
        while(offset < str.size()){
            ssize_t bytes_written = write(fd, str.data() + offset, str.size() - offset);
            if(bytes_written < 0){
                if(errno == EINTR){
                    // interrupted, not a fatal issue
                    continue;
                }
                perror("write");
                return false;
            }
            offset += static_cast<size_t>(bytes_written);
        }
        return true;
    }

}

bool Cachelet::AOF::should_rewrite() const{
    // Rule: Rewrite if the size of the log is larger than the minimum (64 MB)
    // And it's more than 2x larger than the size on last rewrite
    // And it's been more than REWRITE_RETRY_DELAY seconds since our last rewrite attempt
    return last_rewrite_attempt  + REWRITE_RETRY_DELAY <= std::chrono::steady_clock::now() &&
    log_size >= MIN_REWRITE_SIZE && log_size > 2 * size_on_last_rewrite;
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
        path(path),
        needs_fsync(false),
        last_rewrite_attempt(std::chrono::steady_clock::time_point::min()){ 
    fd = open(path.data(), O_CREAT | O_APPEND | O_WRONLY, 0644); // should be write only?
    // note: 0644 is the permissions in octal, not really too important for single user machine
    if(fd == -1){
        throw std::system_error(errno, std::generic_category(),
                                        "failed to open AOF at " +path);
    }

    struct stat st;
    if(fstat(fd, &st) == -1){ // get file size of log
        int err = errno;
        close(fd);
        throw std::system_error(err, std::generic_category(),
                                        "failed to call fstat on newly opened file");
    }
    
    log_size = static_cast<std::size_t>(st.st_size);
    size_on_last_rewrite = log_size;

}

Cachelet::AOF::~AOF(){
    close(fd);
}

void Cachelet::AOF::append_command(const std::vector<std::string> &command){
    std::string encode_result = Cachelet::resp::encode_array(command);
    buffer.append(encode_result);
}

bool Cachelet::AOF::flush(){
    // Architecture: buffer --write()--> page cache --fsync()--> disk
    // always writes contents of the buffer to file, but fsync (because it's relatively slow)
    // frequency is determined by user-set policy
    if(buffer.empty() && !(needs_fsync)){
        // for performance, don't want to run all the code below if no state change in cache
        return true;
    }
    if(!buffer.empty()){
        if(!write_all(fd, buffer)){
            return false;
        }
        log_size += buffer.size();
        needs_fsync = true;
        buffer.clear();
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

auto Cachelet::AOF::rewrite(const Cache &cache) -> RewriteResult{
    std::string output; // to be written to the temp file, essentially what we want to rewrite our AOF log to
    // later, we can periodically write output to the temp file once it exceeds 1 MB, for performance

    cache.for_each([&](const std::string &key, const std::string &value, 
    std::optional<std::chrono::steady_clock::time_point> expire){
        if(expire && *expire <= std::chrono::steady_clock::now()){
            return;
        }
        std::vector<std::string> command = {"SET", key, value};
        if(expire){
            // ttl, so calculate PXAT (e.g. the system clock epoch-based time at which this key expires)
            auto time_left = *expire - std::chrono::steady_clock::now();
            auto expire_at = std::chrono::system_clock::now() + time_left;
            auto expire_ms = std::chrono::duration_cast<std::chrono::milliseconds>
            (expire_at.time_since_epoch()).count();
            command.push_back("PXAT");
            command.push_back(std::to_string(expire_ms));
        }
        output += Cachelet::resp::encode_array(command);
    });

    std::string temp_path = path + ".tmp";
    int temp_fd = open(temp_path.data(), O_WRONLY | O_CREAT | O_TRUNC, 0644); // TRUNC to ensure partly-written files don't get mixed in
    if(temp_fd == -1){
        perror("open");
        last_rewrite_attempt = std::chrono::steady_clock::now();
        return RewriteResult::failed_safe;
    }
    if(!write_all(temp_fd, output)){
        // already perrors in write_all
        close(temp_fd);
        unlink(temp_path.data()); // analagous to rm
        last_rewrite_attempt = std::chrono::steady_clock::now();
        return RewriteResult::failed_safe;
    }
    if(fsync(temp_fd) == -1){
        perror("fsync");
        close(temp_fd);
        unlink(temp_path.data());
        last_rewrite_attempt = std::chrono::steady_clock::now();
        return RewriteResult::failed_safe;
    }
    close(temp_fd);

    if(rename(temp_path.c_str(), path.c_str()) == -1){ // WLOG renames "cachelet.aof.tmp" to "cachelet.aof", replaces old aof with rewrite 
        perror("rename");
        unlink(temp_path.data());
        last_rewrite_attempt = std::chrono::steady_clock::now();
        return RewriteResult::failed_safe;
    }
    // from this point forward the old AOF log has been permanently changed
    // we now fsync the directory itself to ensure it points to the now renamed file
    std::string dir = std::filesystem::path(path).parent_path().string();
    if(dir.empty()){
        dir = "."; // if no directory given, we can just resort to the relative directory the command was called from
    }
    int dir_fd = open(dir.c_str(), O_RDONLY | O_DIRECTORY); // read only and ensure called on a directory
    if(dir_fd == -1){
        perror("open dir");
        return RewriteResult::failed_fatal;
    }
    if(fsync(dir_fd) == -1){
        perror("fsync dir");
        close(dir_fd);
        return RewriteResult::failed_fatal;
    }
    close(dir_fd);
    int new_fd = open(path.c_str(), O_CREAT | O_APPEND | O_WRONLY, 0644);
    if(new_fd == -1){
        perror("open [reopen after rewrite]");
        return RewriteResult::failed_fatal;
    }
    close(fd);
    fd = new_fd;
    needs_fsync = false; // we already fsynced before the rename
    log_size = output.size();
    size_on_last_rewrite = output.size();
    last_rewrite_attempt = std::chrono::steady_clock::now();
    return RewriteResult::ok;
}