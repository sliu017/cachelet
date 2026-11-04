#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace Cachelet {
    class Cache;
    class AOF {
        public:
        enum class Policy {no, everysec, always}; // plan to add more eventually
        enum class RewriteResult {ok, failed_safe, failed_fatal};

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
        auto rewrite(const Cache &cache)->RewriteResult;
        bool should_rewrite() const; // allows main to determine when to write the AOF without accessing internal counters

        private:
        int fd; // file descriptor to write to
        Cachelet::AOF::Policy policy;
        std::chrono::steady_clock::time_point last_fsync; // note time of the last fsync, CONSTRUCT TO NOW()
        std::string buffer; // holds commands appended during the current iteration
        std::string path;
        std::size_t log_size; // keeps track of current AOF log size, pretty convenient to track b/c of append-only property
        std::size_t size_on_last_rewrite; // keeps track of AOF log size after last compaction rewrite
        static constexpr std::size_t MIN_REWRITE_SIZE = 64 * 1024 * 1024; // 64 MB
        bool needs_fsync;
    };
    
    void replay(const std::string &path, Cachelet::Cache &cache);
    std::optional<AOF::Policy> parse_policy(const std::string &policy_string);

}