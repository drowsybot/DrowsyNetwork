#include <asio.hpp>
#include <drowsynetwork/Server.hpp>
#include <drowsynetwork/Socket.hpp>
#include <drowsynetwork/PacketBase.hpp>
#include <drowsynetwork/Logging.hpp>
#include <thread>
#include <map>
#include <ranges>

class ConnectionManager {
public:
    void OnConnect(std::shared_ptr<DrowsyNetwork::Socket> Socket) {
        std::lock_guard<std::mutex> Lock(m_Mutex);
        const auto Id = Socket->GetId();
        m_Sockets[Id] = std::move(Socket);
        LOG_INFO("Client {} connected. Total: {}", Id, m_Sockets.size());
    }

    void OnDisconnect(uint64_t Id) {
        std::lock_guard<std::mutex> Lock(m_Mutex);
        m_Sockets.erase(Id);
        LOG_INFO("Client {} disconnected. Total: {}", Id, m_Sockets.size());
    }

    template<DrowsyNetwork::PacketConcept T>
    void Broadcast(const DrowsyNetwork::PacketPtr<T>& Packet) {
        std::lock_guard<std::mutex> Lock(m_Mutex);
        for (auto &Socket: m_Sockets | std::views::values) {
            if (Socket && Socket->IsActive()) {
                Socket->Send(Packet);
            }
        }
    }

private:
    std::mutex m_Mutex;
    std::map<uint64_t, std::shared_ptr<DrowsyNetwork::Socket>> m_Sockets;
};

class MessageSocket : public DrowsyNetwork::Socket {
public:
    MessageSocket(DrowsyNetwork::Executor& IOContext, std::unique_ptr<DrowsyNetwork::TcpSocket>&& Socket, ConnectionManager* Manager)
        : DrowsyNetwork::Socket(IOContext, std::move(Socket)), m_ConnectionManager(Manager), m_LastPacketSize(0) {}

protected:
    void HandleWrite() override {
        if (!IsActive() || m_WriteQueue.empty()) return;

        auto& Packet = m_WriteQueue.front();
        m_LastPacketSize = Packet->size(); // Gimmick to make sure the data is valid until write finishes

        // Send Size prefix + data in one atomic write
        asio::async_write(*m_Socket,
            std::vector<DrowsyNetwork::ConstBuffer>{
                asio::buffer(&m_LastPacketSize, sizeof(m_LastPacketSize)),
                asio::buffer(Packet->data(), Packet->size())
            },
            asio::bind_executor(m_Strand, [self = weak_from_this()](asio::error_code ErrorCode, std::size_t BytesTransferred) {
                if (auto socket = std::static_pointer_cast<MessageSocket>(self.lock())) {
                    socket->FinishWrite(ErrorCode, BytesTransferred);
                }
            })
        );
    }

    void HandleRead() override {
        // Read Size prefix first
        asio::async_read(*m_Socket, m_ReadBuffer, asio::transfer_exactly(sizeof(DrowsyNetwork::SizeType)),
            asio::bind_executor(m_Strand, [self = weak_from_this()](asio::error_code ErrorCode, std::size_t BytesTransferred) {
                if (auto Socket = std::static_pointer_cast<MessageSocket>(self.lock())) {
                    Socket->ReadSize(ErrorCode, BytesTransferred);
                }
            })
        );
    }

    void ReadSize(asio::error_code ErrorCode, std::size_t BytesTransferred) {
        if (!IsActive() || ErrorCode) {
            if (ErrorCode && IsFatalError(ErrorCode))
                Disconnect();
            return;
        }

        // Extract message Size
        auto* SizePtr = static_cast<const DrowsyNetwork::SizeType*>(m_ReadBuffer.data().data());
        auto MessageSize = *SizePtr;

        if (MessageSize > 64 * 1024 * 1024 || MessageSize == 0) {  // 64MB limit
            LOG_ERROR("Invalid message Size: {}", MessageSize);
            Disconnect();
            return;
        }

        m_ReadBuffer.consume(BytesTransferred);

        // Read the actual message
        asio::async_read(*m_Socket, m_ReadBuffer, asio::transfer_exactly(MessageSize),
            asio::bind_executor(m_Strand, [self = weak_from_this()](asio::error_code ErrorCode, size_t BytesTransferred) {
                if (auto Socket = std::static_pointer_cast<MessageSocket>(self.lock())) {
                    Socket->FinishRead(ErrorCode, BytesTransferred);
                }
            })
        );
    }

    void OnRead(const uint8_t* Data, size_t Size) override {
        std::string Message(reinterpret_cast<const char*>(Data), Size);
        LOG_INFO("Socket {} received: '{}'", GetId(), Message);

        // Echo back with prefix
        auto Echo = DrowsyNetwork::PacketBase<std::string>::Create(
            std::format("Echo: {}", Message)
        );
        Send(Echo);
    }

    void OnDisconnect() override {
        m_ConnectionManager->OnDisconnect(GetId());
    }

private:
    ConnectionManager* m_ConnectionManager;
    DrowsyNetwork::SizeType m_LastPacketSize;
};

class MessageServer : public DrowsyNetwork::Server {
public:
    MessageServer(DrowsyNetwork::Executor& IOContext, ConnectionManager* Manager)
        : Server(IOContext), m_ConnectionManager(Manager) {}

private:
    void OnAccept(std::unique_ptr<DrowsyNetwork::TcpSocket>&& Socket) override {
        auto NewSocket = std::make_shared<MessageSocket>(m_IoContext, std::move(Socket), m_ConnectionManager);
        NewSocket->Setup();
        m_ConnectionManager->OnConnect(std::move(NewSocket));
    }

private:
    ConnectionManager* m_ConnectionManager;
};

int main() {
    try {
        asio::io_context IOContext;
        ConnectionManager CManager;
        MessageServer Server(IOContext, &CManager);

        if (!Server.Bind("127.0.0.1", "8080")) {
            LOG_ERROR("Failed to bind to port 8080");
            return 1;
        }

        Server.StartListening();
        LOG_INFO("Message server listening on 127.0.0.1:8080");

        // Graceful shutdown
        asio::signal_set Signals(IOContext, SIGINT, SIGTERM);
        Signals.async_wait([&](auto, auto) {
            LOG_INFO("Shutting down...");
            IOContext.stop();
        });

        // Multi-threaded for better performance
        unsigned int ThreadCount = std::thread::hardware_concurrency();
        std::vector<std::thread> Threads;

        for (unsigned int i = 0; i < ThreadCount; ++i) {
            Threads.emplace_back([&IOContext]() {
                IOContext.run();
            });
        }

        LOG_INFO("Server started with {} threads", ThreadCount);

        for (auto& Thread : Threads) {
            Thread.join();
        }

        return 0;

    } catch (const std::exception& e) {
        LOG_ERROR("Server error: {}", e.what());
        return 1;
    }
}