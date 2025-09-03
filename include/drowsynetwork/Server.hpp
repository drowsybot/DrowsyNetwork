#pragma once

#include "Common.hpp"

namespace DrowsyNetwork {

class Server {
public:
    Server() = delete;
    explicit Server(Executor& IoContext);
    virtual ~Server();

    bool Bind(std::string_view Host, std::string_view Port);
    bool Bind(const TcpEndpoint& Endpoint);

    void StartListening();

    [[nodiscard]] TcpAcceptor* GetAcceptor(size_t Index);

protected:
    [[nodiscard]] TcpAcceptor* CreateAcceptor(const asio::ip::tcp& Protocol);

    void Listen(size_t Index);
    void Accept(size_t Index, TcpSocket&& Socket, asio::error_code ErrorCode);
    void CloseAcceptor(TcpAcceptor& Acceptor);

    virtual void OnAccept(TcpSocket&& Socket) = 0;

protected:
    Executor& m_IoContext;
    std::vector<TcpAcceptor> m_Acceptors;
    TcpResolver m_Resolver;
};

} // namespace DrowsyNetwork