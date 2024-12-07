# Enable CMake profiling option
option(ENABLE_PROFILING "Enable gRPC profiling with cprof" OFF)

#
# Sources
#
set(SOURCES
    src/main.cpp
    src/globals.cpp
    src/consistent_cache_service.cpp
)

#
# Include
#
set(HEADERS
    include/globals.hpp
    include/consistent_cache_service.hpp
)

# Manually find the MySQL Connector C++ library
find_library(MYSQLCPP_CONN_LIBRARY NAMES mysqlcppconn PATHS /usr/lib /usr/local/lib)

# Manually find the MySQL Connector C++ headers
find_path(MYSQLCPP_CONN_INCLUDE_DIR NAMES mysql_driver.h PATHS /usr/include /usr/local/include)

# Add the include directory
include_directories(${MYSQLCPP_CONN_INCLUDE_DIR})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCES})

# Conditionally add profiling settings
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")  # Add the -pg flag for profiling
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")  # Ensure the linker also uses -pg
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -L/usr/lib -lprofiler")  # Add libprofiler linking

# if (DEFINED ENV{ENABLE_PROFILING} AND $ENV{ENABLE_PROFILING} STREQUAL "true")
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")  # Add the -pg flag for profiling
# endif()

# Add the source files
add_executable(server ${SOURCES} ${HEADERS})

# Link the libmemcached library
target_link_libraries(server
    PRIVATE
    myproto
    ${MYSQLCPP_CONN_LIBRARY}
)