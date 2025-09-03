# Raw/Basic server example
add_executable(raw_server_example server_example.cpp)
target_link_libraries(raw_server_example
    PRIVATE
        DrowsyNetwork::DrowsyNetwork
)