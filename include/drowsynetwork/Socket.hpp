#pragma once

#include "Common.hpp"
#include "PacketBase.h"
#include <vector>
#include <queue>
#include <memory>
#include <span>

namespace DrowsyNetwork {

class Socket : std::enable_shared_from_this<Socket> {
public:
    Socket() = delete;
    explicit Socket(Executor& io_context, TcpSocket&& socket);

    TcpSocket& GetSocket();

    virtual void Send(const uint8_t* data, size_t size);

private:
    void DoWrite();

public:
    Strand<ExecutorType> m_Strand;
    TcpSocket m_Socket;
    std::deque<std::span<const uint8_t>> m_WriteQueue;
};

} // namespace DrowsyNetwork