cmake_minimum_required(VERSION 2.8)
project(c10k)

##################
# Compiler setting
##################
# Use C++ 11
set(CMAKE_CXX_FLAGS "-Wall -O3 -std=c++11")
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

########################
# Build executable files
########################
set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")
add_executable(c10k main.cpp BlockingQueue.h Expression.h)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(c10k PRIVATE Threads::Threads)
