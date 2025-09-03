# Examples
add_executable(server_example server_example.cpp)
target_link_libraries(server_example
    PRIVATE
        DrowsyNetwork::DrowsyNetwork
)
