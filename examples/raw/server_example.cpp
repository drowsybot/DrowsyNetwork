#include <asio.hpp>
#include <drowsynetwork/Server.hpp>
#include <drowsynetwork/Socket.hpp>
#include <drowsynetwork/PacketBase.h>
#include <drowsynetwork/Logging.hpp>
#include <thread>
#include <vector>

/**
 * ExampleSocket implements a simple message protocol:
 * 1. Read message size (sizeof(SizeType) bytes)
 * 2. Read message data (size bytes)
 * 
 * Write protocol:
 * 1. Send message size first
 * 2. Send message data
 */
class ExampleSocket : public DrowsyNetwork::Socket {
public:
    ExampleSocket() = delete;
    explicit ExampleSocket(DrowsyNetwork::Executor& io_context, DrowsyNetwork::TcpSocket&& socket) :
        Socket(io_context, std::move(socket)) {
    }
    ~ExampleSocket() override = default;

protected:
    // Override write to include size prefix
    void HandleWrite() override;
    
    // Override read to first read size, then message
    void HandleRead() override;

    // Handle reading the message size (first step)
    void FinishSizeRead(asio::error_code error, std::size_t bytes_transferred);
    
    // Process the actual message data (second step)
    void OnRead(const uint8_t* data, size_t size) override;
};

void ExampleSocket::HandleWrite() {
    if (!IsActive() || m_WriteQueue.empty())
        return;

    auto& instance = m_WriteQueue.front();

    // Send size prefix followed by message data in a single write operation
    // This ensures atomicity and reduces TCP fragmentation
    asio::async_write(m_Socket, std::vector<DrowsyNetwork::ConstBuffer>{
            asio::buffer(&instance.Size, sizeof(DrowsyNetwork::SizeType)),
            asio::buffer(instance.Packet->data(), instance.Packet->size())
        },
        asio::bind_executor(m_Strand, [self = weak_from_this()](asio::error_code error, std::size_t bytes_transferred) mutable {
            if (auto socket = std::static_pointer_cast<ExampleSocket>(self.lock())) {
                socket->FinishWrite(error, bytes_transferred);
            } else {
                // Handle invalid socket
                LOG_ERROR("Invalid socket at handle write");
            }
    }));
}

void ExampleSocket::HandleRead() {
    // First, read exactly the size of our size type to get message length
    asio::async_read(m_Socket, m_ReadBuffer, asio::transfer_exactly(sizeof(DrowsyNetwork::SizeType)),
        asio::bind_executor(m_Strand,
        [self = weak_from_this()](asio::error_code error, std::size_t bytes_transferred) {
            if (auto socket = std::static_pointer_cast<ExampleSocket>(self.lock())) {
                socket->FinishSizeRead(error, bytes_transferred);
            }
        }
    ));
}

void ExampleSocket::FinishSizeRead(asio::error_code error, std::size_t bytes_transferred) {
    if (!IsActive())
        return;

    if (error) {
        LOG_ERROR("Socket {}, read size failed: {}", GetId(), error.message());
        if (IsFatalError(error)) {
            SetActiveStatus(false);
        }
        return;
    }

    if (!bytes_transferred) {
        // No data received, start reading again
        HandleRead();
        return;
    }

    // Extract message size from buffer
    const auto Size = *static_cast<const DrowsyNetwork::SizeType*>(m_ReadBuffer.data().data());
    LOG_DEBUG("Socket {} received size: {}", GetId(), Size);

    // Consume the size bytes from buffer
    m_ReadBuffer.consume(bytes_transferred);

    // Now read the actual message data
    asio::async_read(m_Socket, m_ReadBuffer, asio::transfer_exactly(Size),
        asio::bind_executor(m_Strand,
        [self = weak_from_this()](asio::error_code error, std::size_t bytes_transferred) {
            if (auto socket = std::static_pointer_cast<ExampleSocket>(self.lock())) {
                socket->FinishRead(error, bytes_transferred);
            }
        }
    ));
}

void ExampleSocket::OnRead(const uint8_t* data, size_t size) {
    // Convert received bytes to string for this example
    std::string Message(reinterpret_cast<const char*>(data), size);
    LOG_INFO("Socket {} received message: {} with size: {}", GetId(), Message, size);
}

/**
 * ExampleServer demonstrates:
 * 1. How to accept new connections
 * 2. How to broadcast messages to all connected clients
 * 3. How to manage multiple socket connections
 */
class ExampleServer : public DrowsyNetwork::Server {
public:
    ExampleServer() = delete;
    explicit ExampleServer(asio::io_context& io_context) :
        Server(io_context) {
    }

private:
    // Called when a new client connects
    void OnAccept(DrowsyNetwork::TcpSocket&& socket) override;

private:
    // Keep track of all connected clients
    std::vector<std::shared_ptr<ExampleSocket>> m_Sockets{};
};

void ExampleServer::OnAccept(DrowsyNetwork::TcpSocket&& socket) {
    // Create a notification message for existing clients
    auto welcomePacket = DrowsyNetwork::PacketBase<std::string>::Create();
    welcomePacket->get()->assign(std::format("New connection from {}:{}\n",
        socket.remote_endpoint().address().to_string(),
        socket.remote_endpoint().port()));

    // Broadcast to all existing clients (stress test with 1000 messages)
    for (const auto& existingSocket : m_Sockets) {
        for (int i = 0; i < 1000; ++i) {
            existingSocket->Send(welcomePacket);
        }
    }

    // Create new socket wrapper and start its async operations
    auto newSocket = std::make_shared<ExampleSocket>(m_IoContext, std::move(socket));
    newSocket->Setup(); // Begin async read operations

    // Add to our connection pool
    m_Sockets.emplace_back(std::move(newSocket));
}

int main() {
    asio::io_context io_context;

    // Create and start the server
    ExampleServer server(io_context);
    server.Start();

    // Setup graceful shutdown on SIGINT (Ctrl+C) and SIGTERM
    asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&io_context](const asio::error_code& error, int signal_number) {
        LOG_INFO("Received signal {}, shutting down...", signal_number);
        io_context.stop();
    });

    // Use all available CPU cores for optimal performance
    const unsigned int thread_count = std::max(1u, std::thread::hardware_concurrency());
    LOG_INFO("Starting server with {} threads", thread_count);

    // Create thread pool for handling network I/O
    std::vector<std::thread> thread_pool;
    thread_pool.reserve(thread_count);

    // Launch worker threads - each will process async operations
    for (unsigned int i = 0; i < thread_count; ++i) {
        thread_pool.emplace_back([&io_context, i]() {
            LOG_DEBUG("Worker thread {} started", i);
            try {
                // Run the event loop until io_context.stop() is called
                io_context.run();
            } catch (const std::exception& e) {
                LOG_ERROR("Thread {} error: {}", i, e.what());
            }
            LOG_DEBUG("Worker thread {} finished", i);
        });
    }

    // Wait for all worker threads to complete gracefully
    for (auto& thread : thread_pool) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    LOG_INFO("Server shutdown complete");
    return 0;
}