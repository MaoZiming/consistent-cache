#
# Sources
#
set(SOURCES
    src/client.cpp
    src/main.cpp
    ${CMAKE_SOURCE_DIR}/server/src/globals.cpp
)

set(INCLUDES
    ${CMAKE_SOURCE_DIR}/client/include/client.hpp
    ${CMAKE_SOURCE_DIR}/server/include/globals.hpp
)

include_directories(
    ${CMAKE_SOURCE_DIR}/client/src
    ${CMAKE_SOURCE_DIR}/client/include
    ${CMAKE_SOURCE_DIR}/server/include
    ${CMAKE_SOURCE_DIR}/server/include
)

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${SOURCES})

#
# Target
#
add_executable(
    client
    ${SOURCES}
    ${INCLUDES}
)

target_link_libraries(client
    PRIVATE
    myproto
)
