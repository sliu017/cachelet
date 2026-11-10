cmake_minimum_required(VERSION 3.20)
project(Cachelet VERSION 0.1 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # helps keep vs code consistent with build

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
option(ENABLE_ASAN "Enable AddressSanitizer" ON) # set to off to disable address sanitizer debugging utilities

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/f840327b0f76408eafde2440af3b2f0a70662ea6.zip
    # can update sha to point to latest version
)
FetchContent_MakeAvailable(googletest)

# ---------------------------------------------------------------------------
# cachelet - core library
# ---------------------------------------------------------------------------
add_library(cachelet
    src/cache.cpp
    src/resp.cpp
    src/dispatch.cpp
    src/aof.cpp
    src/eviction_policy.cpp
    src/policies/lru_policy.cpp 
    src/policies/fifo_policy.cpp

)
target_compile_features(cachelet PUBLIC cxx_std_20)
set_target_properties(cachelet PROPERTIES CXX_STANDARD_REQUIRED ON)
target_include_directories(cachelet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(cachelet PRIVATE -Wall -Wextra)

if(ENABLE_ASAN)
    # PUBLIC: propagates to every target that links cachelet
    target_compile_options(cachelet PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(cachelet PUBLIC -fsanitize=address,undefined)
endif()

# ---------------------------------------------------------------------------
# cachelet-server
# ---------------------------------------------------------------------------
add_executable(cachelet-server server/main.cpp)
target_compile_options(cachelet-server PRIVATE -Wall -Wextra)
target_link_libraries(cachelet-server PRIVATE cachelet)

# ---------------------------------------------------------------------------
# cachelet_sim_lib - harness code, keep as a library for the simulation and unit tests on its functionality
# ---------------------------------------------------------------------------
add_library(cachelet_sim_lib sim/simulator.cpp)
target_include_directories(cachelet_sim_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/sim)
target_compile_options(cachelet_sim_lib PRIVATE -Wall -Wextra)
target_link_libraries(cachelet_sim_lib PUBLIC cachelet)

# ---------------------------------------------------------------------------
# cachelet-sim - use a list of traces to simulate real cache activity. user chooses the policy and gets the hits/misses from the sim
# ---------------------------------------------------------------------------
add_executable(cachelet-sim sim/main.cpp)
target_compile_options(cachelet-sim PRIVATE -Wall -Wextra)
target_link_libraries(cachelet-sim PRIVATE cachelet_sim_lib) # brings in cachelet too (PUBLIC above)

# ---------------------------------------------------------------------------
# cache_test - unit tests
# ---------------------------------------------------------------------------
add_executable(cache_test)
target_sources(cache_test
    PRIVATE # previously public
        tests/unit/cache_test.cpp
        tests/unit/resp_test.cpp
        tests/unit/dispatch_test.cpp
        tests/unit/aof_test.cpp
        tests/unit/policies/lru_policy_test.cpp # when we begin introducing multiple tests, we will ensure our interface interacts correctly with each
        tests/unit/simulator_test.cpp
)
target_compile_options(cache_test PRIVATE -Wall -Wextra)
target_link_libraries(cache_test PUBLIC cachelet cachelet_sim_lib GTest::gtest_main)