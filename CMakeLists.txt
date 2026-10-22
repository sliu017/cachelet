cmake_minimum_required(VERSION 3.20)
project(Cachelet VERSION 0.1 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # helps keep vs code consistent with build
add_library(cachelet
    src/cache.cpp 
    src/resp.cpp 
    src/dispatch.cpp
    src/aof.cpp
)
add_executable(cache_test)
add_executable(cachelet-server server/main.cpp)
target_compile_options(cachelet PRIVATE -Wall -Wextra)
target_compile_options(cachelet-server PRIVATE -Wall -Wextra)
target_compile_options(cache_test PRIVATE -Wall -Wextra)
target_sources(cache_test
    PUBLIC
        tests/cache_test.cpp
        tests/resp_test.cpp
        tests/dispatch_test.cpp
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
target_link_libraries(cachelet-server PRIVATE cachelet)

option(ENABLE_ASAN "Enable AddressSanitizer" ON) # set to off to disable address sanitizer debugging utilities
if(ENABLE_ASAN)
    target_compile_options(cachelet PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(cachelet PUBLIC -fsanitize=address,undefined)
endif()
