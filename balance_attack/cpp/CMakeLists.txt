cmake_minimum_required(VERSION 3.15)
project(cpp)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++abi")

set(CMAKE_BUILD_TYPE RELWITHDEBINFO)
#set(CMAKE_BUILD_TYPE DEBUG)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wconversion -stdlib=libc++") # -pedantic
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g -O2")
#set(CMAKE_CXX_FLAGS_DEBUG  "-g")
set(CMAKE_CXX_FLAGS_DEBUG  "-g -fsanitize=memory -fsanitize-memory-track-origins -fsanitize-blacklist=msan_blacklist.txt")
set(CMAKE_CXX_STANDARD 17)

# see https://cmake.org/cmake/help/latest/module/FindBoost.html
find_package(Boost REQUIRED COMPONENTS system)

include_directories(${Boost_INCLUDEDIR})
include_directories(.)

add_executable(cpp
        all.h
        network.cpp
        simulator.cpp main.cpp adversary.cpp adversary.h types.h node_local_view.h simulator.hpp)

# Note: a target should be already defined using 'add_executable' or 'add_library'
target_link_libraries(cpp ${Boost_LIBRARIES} pthread)
