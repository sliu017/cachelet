



/**
 *  private:
        // these generally mirror how they're implemented in libCacheSim. see:
        // https://github.com/1a1a11a/libCacheSim/blob/develop/libCacheSim/cache/eviction/S3FIFO.c
        std::size_t small_capacity; // = 0.1 * capacity;
        std::size_t main_capacity; // = capacity - small_queue_size;
        std::size_t ghost_capacity; // = 0.9 * capacity;
        std::size_t promote_to_main_threshold = 2; // freq >= threshold is the condition for promoting small->main

        enum class Queue {Small, Main};

        struct ListEntry {
            std::string key;
            std::uint8_t freq = 0; // should be capped at 3
        };
        struct MapEntry {
            Queue which;
            std::list<ListEntry>::iterator entry;
        };
        std::list<ListEntry> small_queue; // target size is small_capacity, evict from S when this is exceeded
        std::list<ListEntry> main_queue; // uses all the space small doesn't
        std::list<std::string> ghost_queue; // capped at 90% of total capacity

        std::unordered_map<std::string, MapEntry> key_to_list_entry; // maps to elements of small and main queues
        std::unordered_set<std::string> ghost_set; // incl all elements in the ghost queue, allows membership checks

 */