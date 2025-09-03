#pragma once

#include "Common.hpp"

namespace DrowsyNetwork {

class Server {
public:
    virtual ~Server() = default;

    Server() = delete;
    explicit Server(Executor& io_context);

    void Start();
    void Accept(TcpSocket&& socket, ErrorCode error);

private:
    virtual void OnAccept(TcpSocket&& socket) = 0;

protected:
    Executor& m_IoContext;
    TcpAcceptor m_Acceptor;
};

} // namespace DrowsyNetwork