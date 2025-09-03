include(FetchContent)

FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-34-2
)

FetchContent_MakeAvailable(asio)

# Create interface library for standalone asio
add_library(asio_standalone INTERFACE)
target_include_directories(asio_standalone
    INTERFACE
        ${asio_SOURCE_DIR}/asio/include
)
target_compile_definitions(asio_standalone
    INTERFACE
        ASIO_STANDALONE
)

# Windows needs this
if(WIN32)
    target_compile_definitions(asio_standalone
        INTERFACE
            _WIN32_WINNT=0x0601
    )
endif()

add_library(asio::asio ALIAS asio_standalone)