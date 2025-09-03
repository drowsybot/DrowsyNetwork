#include <asio.hpp>
#include <drowsynetwork/Server.hpp>
#include <drowsynetwork/Socket.hpp>
#include <drowsynetwork/PacketBase.h>

class ExampleServer : public DrowsyNetwork::Server {
public:
    ExampleServer() = delete;
    explicit ExampleServer(asio::io_context& io_context) :
        Server(io_context) {
    }

private:
    void OnAccept(DrowsyNetwork::TcpSocket&& socket) override;

private:
    std::vector<std::shared_ptr<DrowsyNetwork::Socket>> m_Sockets{};
};

void ExampleServer::OnAccept(DrowsyNetwork::TcpSocket&& socket) {
    auto* message = new std::string();
    message->assign(std::format("New connection from {}:{}",
        socket.remote_endpoint().address().to_string(),
        socket.remote_endpoint().port()));

    for (auto& instance : m_Sockets) {
        for (auto i  = 0; i < 1000; ++i)
            instance->Send(reinterpret_cast<const uint8_t*>(message->data()), message->size());
    }

    auto instance = std::make_shared<DrowsyNetwork::Socket>(m_IoContext, std::move(socket));

    m_Sockets.emplace_back(std::move(instance));
}

int main() {
    asio::io_context io_context;

    ExampleServer server(io_context);

    server.Start();

    io_context.run();

    return 0;
}