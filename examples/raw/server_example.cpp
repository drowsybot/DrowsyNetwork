#include <asio.hpp>
#include <drowsynetwork/Server.hpp>
#include <drowsynetwork/Socket.hpp>
#include <drowsynetwork/PacketBase.hpp>
#include <drowsynetwork/Logging.hpp>
#include <thread>
#include <map>
#include <ranges>

class ExampleSocket;

class ConnectionManager {
public:
    ConnectionManager() = default;
    virtual ~ConnectionManager() = default;

public:
    void OnConnect(std::shared_ptr<ExampleSocket>&& Socket);
    void OnDisconnect(uint64_t SocketId);

protected:
    std::map<uint64_t, std::shared_ptr<ExampleSocket>> m_Sockets;
};

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
    explicit ExampleSocket(DrowsyNetwork::Executor& IoContext, DrowsyNetwork::TcpSocket&& socket,
        ConnectionManager* connectionManager) :
        Socket(IoContext, std::move(socket)),
        m_LastWriteSize(0),
        m_ConnectionManager(connectionManager) {
    }
    ~ExampleSocket() override {
        LOG_DEBUG("Socket {} destroyed.", GetId());
    }

protected:
    // Override write to include size prefix
    void HandleWrite() override;
    
    // Override read to first read size, then message
    void HandleRead() override;

    // Handle reading the message size (first step)
    void FinishSizeRead(asio::error_code ErrorCode, std::size_t BytesTransferred);
    
    // Process the actual message data (second step)
    void OnRead(const uint8_t* Data, size_t Size) override;

    // State management
    void OnDisconnect() override;

protected:
    DrowsyNetwork::SizeType m_LastWriteSize;
    ConnectionManager* m_ConnectionManager;
};

void ExampleSocket::HandleWrite() {
    if (!IsActive() || m_WriteQueue.empty())
        return;

    auto& Instance = m_WriteQueue.front();

    m_LastWriteSize = Instance->size();

    // Send size prefix followed by message data in a single write operation
    // This ensures atomicity and reduces TCP fragmentation
    asio::async_write(m_Socket, std::vector<DrowsyNetwork::ConstBuffer>{
            asio::buffer(&m_LastWriteSize, sizeof(DrowsyNetwork::SizeType)),
            asio::buffer(Instance->data(), m_LastWriteSize)
        },
        asio::bind_executor(m_Strand, [self = weak_from_this()](asio::error_code ErrorCode, std::size_t BytesTransferred) mutable {
            if (auto Socket = std::static_pointer_cast<ExampleSocket>(self.lock())) {
                Socket->FinishWrite(ErrorCode, BytesTransferred);
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
        [self = weak_from_this()](asio::error_code ErrorCode, std::size_t BytesTransferred) {
            if (auto Socket = std::static_pointer_cast<ExampleSocket>(self.lock())) {
                Socket->FinishSizeRead(ErrorCode, BytesTransferred);
            }
        }
    ));
}

void ExampleSocket::FinishSizeRead(asio::error_code ErrorCode, std::size_t BytesTransferred) {
    if (!IsActive())
        return;

    if (ErrorCode) {
        LOG_ERROR("Socket {}, read size failed: {}", GetId(), ErrorCode.message());
        if (IsFatalError(ErrorCode) && IsActive()) {
            Disconnect();
        } else if (IsActive()) {
            HandleRead();
        }
        return;
    }

    if (!BytesTransferred) {
        // No data received, start reading again
        HandleRead();
        return;
    }

    // Extract message size from buffer
    const auto Size = *static_cast<const DrowsyNetwork::SizeType*>(m_ReadBuffer.data().data());
    LOG_DEBUG("Socket {} received size: {}", GetId(), Size);

    // Consume the size bytes from buffer
    m_ReadBuffer.consume(BytesTransferred);

    // Now read the actual message data
    asio::async_read(m_Socket, m_ReadBuffer, asio::transfer_exactly(Size),
        asio::bind_executor(m_Strand,
        [self = weak_from_this()](asio::error_code ErrorCode, std::size_t BytesTransferred) {
            if (auto Socket = std::static_pointer_cast<ExampleSocket>(self.lock())) {
                Socket->FinishRead(ErrorCode, BytesTransferred);
            }
        }
    ));
}

void ExampleSocket::OnRead(const uint8_t* Data, size_t Size) {
    // Convert received bytes to string for this example
    std::string Message(reinterpret_cast<const char*>(Data), Size);
    LOG_INFO("Socket {} received message: {} with size: {}", GetId(), Message, Size);
}

void ExampleSocket::OnDisconnect() {
    // In this case, m_ConnectionManager will never be dangling or nullptr
    m_ConnectionManager->OnDisconnect(GetId());
}

/**
 * ExampleServer demonstrates:
 * 1. How to accept new connections
 * 2. How to broadcast messages to all connected clients
 * 3. How to manage multiple socket connections
 */
class ExampleServer : public DrowsyNetwork::Server, public std::enable_shared_from_this<ExampleServer> {
public:
    ExampleServer() = delete;
    explicit ExampleServer(asio::io_context& IoContext, ConnectionManager* connectionManager) :
        Server(IoContext),
        m_ConnectionManager(connectionManager) {
    }

private:
    // Called when a new client connects
    void OnAccept(DrowsyNetwork::TcpSocket&& Socket) override;

private:
    ConnectionManager* m_ConnectionManager;
};

void ExampleServer::OnAccept(DrowsyNetwork::TcpSocket&& Socket) {
    // Create new socket wrapper and start its async operations
    auto newSocket = std::make_shared<ExampleSocket>(m_IoContext, std::move(Socket), m_ConnectionManager);
    newSocket->Setup(); // Begin async read operations

    m_ConnectionManager->OnConnect(std::move(newSocket));
}

void ConnectionManager::OnConnect(std::shared_ptr<ExampleSocket>&& Socket) {
    // Create a notification message for existing clients
    auto WelcomePacket = DrowsyNetwork::PacketBase<std::string>::Create();
    WelcomePacket->get()->assign(std::format("New connection from {}:{}\n",
        Socket->GetSocket().remote_endpoint().address().to_string(),
        Socket->GetSocket().remote_endpoint().port()));

    // Broadcast to all existing clients (stress test with 1000 messages)
    for (const auto &existingSocket: m_Sockets | std::views::values) {
        for (int i = 0; i < 1'000'0; ++i) {
            existingSocket->Send(WelcomePacket);
        }
    }

    // Add to our connection pool
    m_Sockets.emplace(Socket->GetId(), std::move(Socket));
}

void ConnectionManager::OnDisconnect(uint64_t SocketId) {
    m_Sockets.erase(SocketId);
}

int main() {
    asio::io_context IoContext;

    auto connectionManager = std::make_unique<ConnectionManager>();

    // Create and start the server
    ExampleServer server = ExampleServer(IoContext, connectionManager.get());

    if (!server.Bind("::1", "8080"))
        return 0;

    if (!server.Bind("192.168.1.139", "8080"))
        return 0;

    if (!server.Bind("0.0.0.0", "8080"))
        return 0;

    server.StartListening();

    // Setup graceful shutdown on SIGINT (Ctrl+C) and SIGTERM
    asio::signal_set signals(IoContext, SIGINT, SIGTERM);
    signals.async_wait([&IoContext](const asio::error_code& error, int signal_number) {
        LOG_INFO("Received signal {}, shutting down...", signal_number);
        IoContext.stop();
    });

    // Use all available CPU cores for optimal performance
    const unsigned int ThreadCount = std::max(1u, std::thread::hardware_concurrency());
    LOG_INFO("Starting server with {} threads", ThreadCount);

    // Create thread pool for handling network I/O
    std::vector<std::thread> ThreadPool;
    ThreadPool.reserve(ThreadCount);

    // Launch worker threads - each will process async operations
    for (unsigned int i = 0; i < ThreadCount; ++i) {
        ThreadPool.emplace_back([&IoContext, i]() {
            LOG_DEBUG("Worker thread {} started", i);
            try {
                // Run the event loop until io_context.stop() is called
                IoContext.run();
            } catch (const std::exception& e) {
                LOG_ERROR("Thread {} error: {}", i, e.what());
            }
            LOG_DEBUG("Worker thread {} finished", i);
        });
    }

    // Wait for all worker threads to complete gracefully
    for (auto& Thread : ThreadPool) {
        if (Thread.joinable()) {
            Thread.join();
        }
    }

    LOG_INFO("Server shutdown complete");
    return 0;
}