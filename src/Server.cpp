#include <memory>
#include "drowsynetwork/Server.hpp"
#include "drowsynetwork/Logging.hpp"

namespace DrowsyNetwork {

Server::Server(Executor& IOContext) :
    m_IoContext(IOContext),
    m_Resolver(IOContext)
{
}

Server::~Server() {
    for (auto& Acceptor : m_Acceptors) {
        CloseAcceptor(Acceptor);
    }

    m_Acceptors.clear(); // Not required since it's the deconstructor but let's show intent
}

bool Server::Bind(std::string_view Host, std::string_view Port) {
    asio::error_code ErrorCode;
    auto Endpoints = m_Resolver.resolve(Host, Port, ErrorCode);
    if (ErrorCode) {
        LOG_ERROR("Binding {}:{} has failed: ({}) - {}", Host.data(), Port.data(), ErrorCode.value(), ErrorCode.message());
        return false;
    }

    bool BoundToAtLeastOne = false;
    for (const auto& Entry : Endpoints) {
        const auto& Endpoint = Entry.endpoint();

        if (Bind(Endpoint)) {
            LOG_DEBUG("Server listening on {}:{}", Endpoint.address().to_string(), Endpoint.port());
            BoundToAtLeastOne = true;
        }
    }

    return BoundToAtLeastOne;
}

bool Server::Bind(const TcpEndpoint& Endpoint) {
    auto Acceptor = CreateAcceptor(Endpoint.protocol());
    if (!Acceptor) {
        LOG_ERROR("Failed to create acceptor.");
        return false;
    }

    asio::error_code ErrorCode;
    Acceptor->bind(Endpoint, ErrorCode);
    if (ErrorCode) {
        LOG_ERROR("Error binding to endpoint {}:{}: ({}) - {}.", Endpoint.address().to_string(), Endpoint.port(),
            ErrorCode.value(), ErrorCode.message());
        CloseAcceptor(*Acceptor);
        m_Acceptors.pop_back();
        return false;
    }

    return true;
}

void Server::StartListening() {
    asio::error_code ErrorCode;
    for (size_t Index = 0; Index < m_Acceptors.size(); ++Index) {
        auto& Acceptor = m_Acceptors.at(Index);

        if (!Acceptor.is_open())
            continue;

        Acceptor.listen(asio::socket_base::max_listen_connections, ErrorCode);
        if (ErrorCode) {
            LOG_ERROR("Failed to start listening on acceptor {}: ({}) - {}", Index, ErrorCode.value(), ErrorCode.message());
            continue;
        }

        Listen(Index);
    }
}

void Server::Listen(size_t Index) {
    auto Acceptor = GetAcceptor(Index);
    if (!Acceptor) {
        LOG_ERROR("Failed to get acceptor {} while listening for connections.", Index);
        return;
    }

    auto Socket = std::make_unique<TcpSocket>(m_IoContext);
    Acceptor->async_accept(*Socket,
    [this, Socket = std::move(Socket), Index](asio::error_code ErrorCode) mutable {
            Accept(Index, std::move(Socket), ErrorCode);
        });
}

void Server::Accept(size_t Index, std::unique_ptr<TcpSocket>&& Socket, asio::error_code ErrorCode) {
    if (!ErrorCode) {
        LOG_DEBUG("Accepting socket from acceptor: {}", Index);
        OnAccept(std::move(Socket));
    } else {
        LOG_ERROR("Accept failed for acceptor {}: ({}) - {}", Index, ErrorCode.value(), ErrorCode.message());
    }

    Listen(Index);
}

TcpAcceptor* Server::GetAcceptor(size_t Index) {
    if (Index >= m_Acceptors.size())
        return nullptr;

    return &m_Acceptors.at(Index);
}

TcpAcceptor* Server::CreateAcceptor(const asio::ip::tcp& Protocol) {
    TcpAcceptor Acceptor(m_IoContext);

    asio::error_code ErrorCode;
    Acceptor.open(Protocol, ErrorCode);

    Acceptor.set_option(asio::socket_base::reuse_address(true));

    if (Protocol == asio::ip::tcp::v6()) {
        Acceptor.set_option(asio::ip::v6_only(true));
    }

    if (ErrorCode) {
        LOG_ERROR("Error opening acceptor type {}: ({}) - {}", Protocol == asio::ip::tcp::v4() ? "IPv4" : "IPv6", ErrorCode.value(), ErrorCode.message());
        return nullptr;
    }

    return &m_Acceptors.emplace_back(std::move(Acceptor));
}

void Server::CloseAcceptor(TcpAcceptor& Acceptor) {
    if (!Acceptor.is_open())
        return;

    asio::error_code ErrorCode;
    Acceptor.cancel(ErrorCode);
    Acceptor.close(ErrorCode);

    if (ErrorCode) {
        LOG_ERROR("Error closing acceptor: ({}) - {}", ErrorCode.value(), ErrorCode.message());
    }
}

} // namespace DrowsyNetwork