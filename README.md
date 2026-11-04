# Cachelet (WIP)

An in-memory cache written in C++20. Built to develop a deeper understanding of low-level design, systems programming, concurrency, and other concepts I plan to learn throughout the semester/year.

# Features

Currently has support for RESP and works with `redis-cli` and `redis-benchmark`, with commands GET, SET, DEL, DBSIZE, and PING. Underlying cache uses LRU eviction policy through `std::list`. (might explore more intriguing policies at a later date). Also supports TTL with lazy expiry.

Learned basic socket programming using [Beej's guide](https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf) to set up the cache's server, and implemented epoll to handle multiple clients at once.

Persistence: uses an append-only file (AOF) to log commands that change the state of the cache, such that we can recreate its contents upon startup. Additionally, this is periodically rewritten to use only the cache's current items, while maintaining safety against crashes.

Currently tackling exploring different (possibly novel?) eviction policies and building a policy interface.

# Benchmarks

Matches Redis throughput for small values, with up to 1,000 clients.  (using redis-benchmark, on WSL2)

| Workload | Cachelet vs Redis  (persistence disabled) |
|---|---|
| Small values, 50 clients (SET / GET) | parity (~155k req/s) |
| Small values, 1,000 clients (SET / GET) | parity (~115k req/s) |
| 100 KB values, GET | ~70% of Redis |
| 100 KB values, SET | ~60% of Redis |

Numbers are similar if persistence is turned on for every policy (with the large value issue still present). Debugging the large value discrepancy is currently planned work.

# How to run

``` bash
# Debug build (with tests)
cmake -B build
cmake --build build
./build/cache_test

# Release build
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
./build-release/cachelet
```

In another terminal: 

``` bash
redis-cli -p 6379
> SET key value EX 10
> GET key
```