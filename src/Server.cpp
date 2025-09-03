#include "drowsynetwork/Server.hpp"

namespace DrowsyNetwork {

Server::Server(Executor& io_context) :
    m_IoContext(io_context),
    m_Acceptor(io_context, TcpEndpoint(asio::ip::tcp::v4(), 8080))
{
}

void Server::Start() {
    auto socket = std::make_unique<TcpSocket>(m_IoContext);
    m_Acceptor.async_accept(*socket,
    [this, socket = std::move(socket)](ErrorCode error) mutable {
            Accept(std::move(*socket), error);
        });
}

void Server::Accept(TcpSocket&& socket, ErrorCode error) {
    if (!error) {
        OnAccept(std::move(socket));
    }

    Start();
}

} // namespace DrowsyNetwork