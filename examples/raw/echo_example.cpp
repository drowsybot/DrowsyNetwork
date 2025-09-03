#include <asio.hpp>
#include <drowsynetwork/Server.hpp>
#include <drowsynetwork/Socket.hpp>
#include <drowsynetwork/PacketBase.hpp>
#include <drowsynetwork/Logging.hpp>
#include <thread>
#include <map>

class ConnectionManager {
public:
    void AddSocket(uint64_t id, std::shared_ptr<DrowsyNetwork::Socket> Socket) {
        m_sockets[id] = std::move(Socket);
        LOG_INFO("Client {} connected. Total connections: {}", id, m_sockets.size());
    }

    void RemoveSocket(uint64_t id) {
        m_sockets.erase(id);
        LOG_INFO("Client {} disconnected. Total connections: {}", id, m_sockets.size());
    }

private:
    std::map<uint64_t, std::shared_ptr<DrowsyNetwork::Socket>> m_sockets;
};

class EchoSocket : public DrowsyNetwork::Socket {
public:
    EchoSocket(DrowsyNetwork::Executor& ioContext, DrowsyNetwork::TcpSocket&& socket, ConnectionManager* manager)
        : Socket(ioContext, std::move(socket)), m_manager(manager) {}

protected:
    void OnRead(const uint8_t* data, size_t size) override {
        // Echo the data back
        std::vector<uint8_t> echoData(data, data + size);
        auto packet = DrowsyNetwork::PacketBase<std::vector<uint8_t>>::Create(std::move(echoData));
        Send(packet);
    }

    void OnDisconnect() override {
        m_manager->RemoveSocket(GetId());
    }

private:
    ConnectionManager* m_manager;
};

class EchoServer : public DrowsyNetwork::Server {
public:
    EchoServer(asio::io_context& ioContext, ConnectionManager* manager)
        : Server(ioContext), m_manager(manager) {}

private:
    void OnAccept(DrowsyNetwork::TcpSocket&& socket) override {
        auto echoSocket = std::make_shared<EchoSocket>(m_IoContext, std::move(socket), m_manager);
        echoSocket->Setup();

        // Register with connection manager
        m_manager->AddSocket(echoSocket->GetId(), echoSocket);
    }

private:
    ConnectionManager* m_manager;
};

int main() {
    try {
        asio::io_context ioContext;
        ConnectionManager manager;
        EchoServer server(ioContext, &manager);

        // Bind to localhost
        if (!server.Bind("127.0.0.1", "8080")) {
            LOG_ERROR("Failed to bind to port 8080");
            return 1;
        }

        server.StartListening();
        LOG_INFO("Echo server listening on 127.0.0.1:8080");
        LOG_INFO("Test with: telnet 127.0.0.1 8080");

        // Graceful shutdown
        asio::signal_set signals(ioContext, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto) {
            LOG_INFO("Shutting down...");
            ioContext.stop();
        });

        // Single thread for simplicity
        ioContext.run();

        LOG_INFO("Server stopped");
        return 0;

    } catch (const std::exception& e) {
        LOG_ERROR("Server error: {}", e.what());
        return 1;
    }
}