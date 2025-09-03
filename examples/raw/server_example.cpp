#include <asio.hpp>
#include <drowsynetwork/Server.hpp>
#include <drowsynetwork/Socket.hpp>
#include <drowsynetwork/PacketBase.h>
#include <drowsynetwork/Logging.hpp>
#include <thread>
#include <vector>

class ExampleSocket : public DrowsyNetwork::Socket {
public:
    ExampleSocket() = delete;
    explicit ExampleSocket(DrowsyNetwork::Executor& io_context, DrowsyNetwork::TcpSocket&& socket) :
        Socket(io_context, std::move(socket)) {
    }
    ~ExampleSocket() override = default;

protected:
    void HandleWrite() override;
    void HandleRead() override;

    void FinishSizeRead(asio::error_code error, std::size_t bytes_transferred);
    void OnRead(const uint8_t* data, size_t size) override;
};

void ExampleSocket::HandleWrite() {
    if (!IsActive() || m_WriteQueue.empty())
        return;

    auto& instance = m_WriteQueue.front();

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
        HandleRead();
        return;
    }

    const auto Size = *static_cast<const DrowsyNetwork::SizeType*>(m_ReadBuffer.data().data());

    LOG_DEBUG("Socket {} received size: {}", GetId(), Size);

    m_ReadBuffer.consume(bytes_transferred);

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
    std::string Message(reinterpret_cast<const char*>(data), size);
    LOG_INFO("Socket {} received message: {} with size: {}", GetId(), Message, size);
}


class ExampleServer : public DrowsyNetwork::Server {
public:
    ExampleServer() = delete;
    explicit ExampleServer(asio::io_context& io_context) :
        Server(io_context) {
    }

private:
    void OnAccept(DrowsyNetwork::TcpSocket&& socket) override;

private:
    std::vector<std::shared_ptr<ExampleSocket>> m_Sockets{};
};

class Test {
public:
    Test() {
        m_Instance = new std::string("Test");
    }
    const uint8_t* GetData() {
        return reinterpret_cast<const uint8_t*>(m_Instance->data());
    }

    size_t GetSize() const {
        return m_Instance->size();
    }

private:
    std::string* m_Instance;
};

void ExampleServer::OnAccept(DrowsyNetwork::TcpSocket&& socket) {
    auto packet = DrowsyNetwork::PacketBase<std::string>::Create();
    packet->get()->assign(std::format("New connection from {}:{}\n",
        socket.remote_endpoint().address().to_string(),
        socket.remote_endpoint().port()));

    for (auto& instance : m_Sockets) {
        for (auto i  = 0; i < 1000; ++i)
            instance->Send(packet);
    }

    auto instance = std::make_shared<ExampleSocket>(m_IoContext, std::move(socket));
    instance->Setup();

    m_Sockets.emplace_back(std::move(instance));
}

int main() {
    asio::io_context io_context;

    ExampleServer server(io_context);
    server.Start();

    // Setup graceful shutdown
    asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&io_context](const asio::error_code& error, int signal_number) {
        LOG_INFO("Received signal {}, shutting down...", signal_number);
        io_context.stop();
    });

    // Calculate optimal thread count (typically CPU cores)
    const unsigned int thread_count = std::max(1u, std::thread::hardware_concurrency());
    LOG_INFO("Starting server with {} threads", thread_count);

    // Create thread pool
    std::vector<std::thread> thread_pool;
    thread_pool.reserve(thread_count);

    // Launch worker threads
    for (unsigned int i = 0; i < thread_count; ++i) {
        thread_pool.emplace_back([&io_context, i]() {
            LOG_DEBUG("Worker thread {} started", i);
            try {
                io_context.run();
            } catch (const std::exception& e) {
                LOG_ERROR("Thread {} error: {}", i, e.what());
            }
            LOG_DEBUG("Worker thread {} finished", i);
        });
    }

    // Wait for all threads to complete
    for (auto& thread : thread_pool) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    LOG_INFO("Server shutdown complete");
    return 0;
}