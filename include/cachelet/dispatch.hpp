#pragma once

#include "cachelet/cache.hpp"

#include <string>
#include <vector>

// dispatch takes a commmand from a ParseResult and runs the commnand, then
// returns the encoded reply bytes (e.g. using resp's encode suite)
namespace Cachelet {
    std::string dispatch(Cachelet::Cache &cache, std::vector<std::string> const &command);
}