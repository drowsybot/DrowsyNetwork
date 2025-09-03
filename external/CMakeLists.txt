FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-34-2
)

FetchContent_MakeAvailable(asio)

# Create interface library for standalone asio
add_library(asio_standalone INTERFACE)
target_include_directories(asio_standalone INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio_standalone INTERFACE ASIO_STANDALONE)

# Create alias for easier linking
add_library(asio::asio ALIAS asio_standalone)