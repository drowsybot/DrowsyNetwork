cmake_minimum_required(VERSION 3.31)

project(DrowsyNetwork
    VERSION 1.0.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build options
option(BUILD_EXAMPLES "Build DrowsyNetwork examples" ON)

# Add external dependencies
add_subdirectory(external)

# Create the main library
add_library(DrowsyNetwork
    src/Socket.cpp
    src/Server.cpp
)

# Add alias for namespace consistency
add_library(DrowsyNetwork::DrowsyNetwork ALIAS DrowsyNetwork)

# Configure target
target_include_directories(DrowsyNetwork
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(DrowsyNetwork
    PUBLIC
        asio::asio
)

set_target_properties(DrowsyNetwork PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    EXPORT_NAME DrowsyNetwork
)

# Build examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()