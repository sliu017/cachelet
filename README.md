# Cachelet (WIP)

An in-memory cache written in C++20. Built to develop a deeper understanding of low-level design, systems programming, concurrency, and other concepts I plan to learn throughout the semester/year.

# Features

Currently has support for RESP and works with `redis-cli` and `redis-benchmark`, with commands GET, SET, DEL, DBSIZE, and PING. Underlying cache uses LRU eviction policy through `std::list`. (might explore more intriguing policies at a later date). Also supports TTL with lazy expiry.

Learned basic socket programming using [Beej's guide](https://beej.us/guide/bgnet/pdf/bgnet_usl_c_1.pdf) to set up the cache's server, and implemented epoll to handle multiple clients at once.

Now tackling persistence!