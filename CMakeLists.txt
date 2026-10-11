cmake_minimum_required(VERSION 3.16)
project(strategy_replications LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Where the library is. The default is the submodule path suggested by the README;
# point it somewhere else with -DALGOTRADELIB_DIR=... if you vendor it elsewhere.
set(ALGOTRADELIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libs/algotradelib"
    CACHE PATH "Path to an algotradelib checkout")

if(NOT EXISTS "${ALGOTRADELIB_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "No algotradelib checkout at ${ALGOTRADELIB_DIR}. Add it as a submodule:\n"
        "    git submodule add https://github.com/ygeunkim/algotradelib.git libs/algotradelib\n"
        "    git submodule update --init --recursive\n"
        "or configure with -DALGOTRADELIB_DIR=/path/to/algotradelib.")
endif()

add_library(strategy_replications INTERFACE)
target_include_directories(strategy_replications INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(strategy_replications INTERFACE algotradelib::algotradelib)

# # A second argument is required because the source directory may sit outside this project tree.
# add_subdirectory("${ALGOTRADELIB_DIR}" algotradelib-build)

# add_executable(strategy_replications src/main.cpp)
# target_link_libraries(strategy_replications PRIVATE algotradelib::algotradelib)
