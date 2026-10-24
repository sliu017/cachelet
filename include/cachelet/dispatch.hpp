#pragma once

#include "cachelet/cache.hpp"

#include <string>
#include <vector>

// dispatch takes a commmand from a ParseResult and runs the commnand, then
// returns the encoded reply bytes (e.g. using resp's encode suite)
namespace Cachelet {
    class AOF;
    
    // overload dispatch based on if we want persistence, e.g. new changes should be committed to the AOF
    std::string dispatch(Cachelet::Cache &cache, const std::vector<std::string> &command);
    std::string dispatch(Cachelet::Cache &cache, AOF& aof, const std::vector<std::string> &command);
}