#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace Cachelet {
    class Cache;
    class AOF {
        public:
        enum class Policy {no, everysec, always}; // plan to add more eventually

        AOF(const std::string &path, Cachelet::AOF::Policy policy);
        ~AOF();
        // disallow copies, because the AOF case is analogous to the raw pointer case 
        // (where copies point to the same spot in memory, leading to incorrect behavior)
        // this time, copies believe they own the same resource (the underlying file descriptor)
        // which will break because the kernel reassigns closed fds 
        // on a deeper level, there's not really a reason to be able to copy AOFs
        AOF(const AOF&) = delete;
        AOF& operator=(const AOF&) = delete;

        void append_command(const std::vector<std::string> &command);
        bool flush();

        private:
        int fd; // file descriptor to write to
        Cachelet::AOF::Policy policy;
        std::chrono::steady_clock::time_point last_fsync; // note time of the last fsync, CONSTRUCT TO NOW()
        std::string buffer; // holds commands appended during the current iteration
        bool needs_fsync;
    };
    
    void replay(const std::string &path, Cachelet::Cache &cache);
    std::optional<AOF::Policy> parse_policy(const std::string &policy_string);

}