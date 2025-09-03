cmake_minimum_required(VERSION 3.31)
project(DrowsyNetwork VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 23)

include(FetchContent)

add_subdirectory(external)

# Create the library
add_library(DrowsyNetwork
    src/Socket.cpp
    src/Server.cpp
)

# Add an alias for namespace consistency
add_library(DrowsyNetwork::DrowsyNetwork ALIAS DrowsyNetwork)

# Set target properties
target_include_directories(DrowsyNetwork
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(DrowsyNetwork 
    PUBLIC
        asio::asio
)

# Set library properties
set_target_properties(DrowsyNetwork PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    EXPORT_NAME DrowsyNetwork
)

# Build examples by default
option(DROWSYNETWORK_BUILD_EXAMPLES "Build DrowsyNetwork examples" ON)

if(DROWSYNETWORK_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
