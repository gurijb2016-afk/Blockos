file(GLOB_RECURSE BLOCKOS_NETWORK_SOURCES
    *.cpp
)

add_library(blockos_network STATIC
    ${BLOCKOS_NETWORK_SOURCES}
)

target_include_directories(blockos_network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
