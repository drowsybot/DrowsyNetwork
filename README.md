# DrowsyNetwork 🌙

A modern, lightweight C++23 networking library built on top of ASIO. DrowsyNetwork provides a simple yet powerful abstraction for building TCP servers and handling socket connections with ease.

## Features ✨

- **Modern C++23**: Takes advantage of the latest C++ features including concepts, ranges, and improved template programming
- **ASIO-powered**: Built on the robust and battle-tested ASIO networking library
- **Thread-safe**: Designed with strand-based concurrency for safe multi-threaded operations
- **Flexible packet system**: Generic packet handling with customizable data types
- **Simple API**: Clean, intuitive interface that gets out of your way
- **Header-mostly**: Minimal compilation overhead with most functionality in headers
- **Logging integration**: Built-in logging system that's easy to customize or replace

## Quick Start 🚀

### Building

```bash
git clone https://github.com/drowsybot/DrowsyNetwork.git
cd DrowsyNetwork
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Basic Echo Server

```cpp
#include <drowsynetwork/Server.hpp>
#include <drowsynetwork/Socket.hpp>
#include <drowsynetwork/PacketBase.hpp>

class EchoSocket : public DrowsyNetwork::Socket {
public:
    EchoSocket(DrowsyNetwork::Executor& ioContext, DrowsyNetwork::TcpSocket&& socket)
        : Socket(ioContext, std::move(socket)) {}

protected:
    void OnRead(const uint8_t* data, size_t size) override {
        // Echo the data back
        std::vector<uint8_t> response(data, data + size);
        auto packet = DrowsyNetwork::PacketBase<std::vector<uint8_t>>::Create(std::move(response));
        Send(packet);
    }
    
    void OnDisconnect() override {
        LOG_INFO("Client {} disconnected", GetId());
    }
};

class EchoServer : public DrowsyNetwork::Server {
public:
    EchoServer(asio::io_context& ioContext) : Server(ioContext) {}

private:
    void OnAccept(DrowsyNetwork::TcpSocket&& socket) override {
        auto client = std::make_shared<EchoSocket>(m_IoContext, std::move(socket));
        client->Setup();
    }
};

int main() {
    asio::io_context ioContext;
    EchoServer server(ioContext);
    
    if (!server.Bind("127.0.0.1", "8080")) {
        return 1;
    }
    
    server.StartListening();
    ioContext.run();
    return 0;
}
```

## Architecture 🏗️

DrowsyNetwork is built around a few core concepts:

### Server
The `Server` class handles accepting incoming connections. You inherit from it and implement `OnAccept()` to handle new connections.

### Socket
The `Socket` class represents individual client connections. It provides:
- Automatic connection management
- Thread-safe sending with strand-based synchronization
- Configurable packet handling
- Built-in error handling and reconnection logic

### PacketBase
A flexible packet system that works with any data type that satisfies the `PacketConcept`. Works with:
- Standard containers (`std::string`, `std::vector`, etc.)
- Custom types with `size()` and `data()` methods
- Legacy types with `GetSize()` and `GetBufferPointer()` methods

## Examples 📚

The `examples/` directory contains several complete examples:

- **Echo Server**: Simple server that echoes back received data
- **Message Server**: More advanced server with message framing and broadcasting
- **Custom Packet Types**: Demonstrates using custom data structures

Run any example:
```bash
cd build/examples
./echo_server
# In another terminal: telnet 127.0.0.1 8080
```

## Requirements 📋

- **C++23 compatible compiler** (GCC 12+, Clang 15+, MSVC 2022+)
- **CMake 3.31+**
- **ASIO** (automatically fetched via CMake)

## Configuration ⚙️

### Logging
By default, DrowsyNetwork uses `std::println` for logging. You can customize this by defining your own logging macros before including the headers:

```cpp
#define LOG_INFO(fmt, ...) my_logger.info(fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) my_logger.error(fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) my_logger.debug(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) my_logger.warn(fmt, ##__VA_ARGS__)

#include <drowsynetwork/Socket.hpp>
```

### CMake Integration

#### Option 1: FetchContent (Recommended)
```cmake
include(FetchContent)

FetchContent_Declare(
    DrowsyNetwork
    GIT_REPOSITORY https://github.com/drowsybot/DrowsyNetwork.git
    GIT_TAG        main  # or specific version tag
)

FetchContent_MakeAvailable(DrowsyNetwork)

target_link_libraries(your_target PRIVATE DrowsyNetwork::DrowsyNetwork)
```

#### Option 2: Add Subdirectory
```cmake
add_subdirectory(DrowsyNetwork)
target_link_libraries(your_target PRIVATE DrowsyNetwork::DrowsyNetwork)
```

## Thread Safety 🔒

DrowsyNetwork is designed to be thread-safe:
- Each socket runs on its own strand to prevent data races
- The `Send()` method can be called from any thread
- Connection management is internally synchronized

## Performance 🏃‍♂️

- **Zero-copy** where possible
- **Efficient memory management** with shared_ptr for packets
- **Strand-based concurrency** eliminates most locking overhead
- **Batch operations** for multiple concurrent connections

## Contributing 🤝

Contributions are welcome! Please feel free to submit pull requests, report bugs, or suggest features.

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests if applicable
5. Submit a pull request

## License 📄

This project is licensed under the MIT License - see the [LICENSE](LICENSE.md) file for details.

## Why "DrowsyNetwork"? 😴

Because sometimes the best networking code is written at 2 AM when you're drowsy but still productive. Also, it aims to make network programming so easy you could do it in your sleep! 💤

---