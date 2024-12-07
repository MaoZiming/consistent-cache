cmake_minimum_required(VERSION 3.13)

project(FreshCache)

# Specify C++ standard
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(server)
add_subdirectory(proto)
add_subdirectory(client)

# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -g")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")  # Add the -pg flag for profiling
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")  # Ensure the linker also uses -pg
