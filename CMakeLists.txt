cmake_minimum_required(VERSION 3.20)
project(Cachelet VERSION 0.1 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # helps keep vs code consistent with build
add_library(cachelet src/cache.cpp)
add_executable(cache_test)
target_sources(cache_test
    PUBLIC
        tests/cache_test.cpp
)

target_compile_features(cachelet PUBLIC cxx_std_20)
set_target_properties(cachelet PROPERTIES CXX_STANDARD_REQUIRED ON)
target_include_directories(cachelet PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/f840327b0f76408eafde2440af3b2f0a70662ea6.zip
    # can update sha to point to latest version
)

FetchContent_MakeAvailable(googletest)
target_link_libraries(cache_test PUBLIC cachelet GTest::gtest_main)

option(ENABLE_ASAN "Enable AddressSanitizer" ON)
if(ENABLE_ASAN)
    target_compile_options(cachelet PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(cachelet PUBLIC -fsanitize=address,undefined)
endif()
