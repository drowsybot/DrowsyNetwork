add_executable(raw_server_example server_example.cpp)
target_link_libraries(raw_server_example
    PRIVATE
        DrowsyNetwork::DrowsyNetwork
)

add_executable(echo_server_example echo_example.cpp)
target_link_libraries(echo_server_example
    PRIVATE
        DrowsyNetwork::DrowsyNetwork
)

