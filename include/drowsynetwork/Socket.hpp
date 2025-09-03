#pragma once

#include "Common.hpp"
#include "PacketBase.hpp"
#include "Logging.hpp"
#include <queue>
#include <memory>
#include <span>
#include <atomic>

namespace DrowsyNetwork {

class Socket : public std::enable_shared_from_this<Socket> {
public:
    Socket() = delete;
    explicit Socket(Executor& IOContext, TcpSocket&& Socket);
    virtual ~Socket() = default;

    TcpSocket& GetSocket();
    uint64_t GetId() const { return m_Id; }

    // Taking a const reference to avoid uselessly incrementing the ref count
    // if answering from within the strand thread
    template <PacketConcept T>
    void Send(const PacketPtr<T>& Packet) {
        if (m_Strand.running_in_this_thread()) {
            EnqueueSend(Packet);
        } else {
            asio::post(m_Strand, [self = weak_from_this(), Packet = Packet]() {
                if (auto socket = self.lock()) {
                    socket->EnqueueSend(Packet);
                } else {
                    // Handle invalid socket
                    LOG_ERROR("Invalid socket at send");
                }
            });
        }
    }

    // To be called after accepting the connection
    virtual void Setup();

    // State management
    void Disconnect();

    bool IsActive() const;

protected:
    template <PacketConcept T>
    void EnqueueSend(const PacketPtr<T>& Packet) {
        if (!IsActive())
            return;

        m_WriteQueue.push_back(Packet);

        if (!m_IsWriting) {
            m_IsWriting = true;
            HandleWrite();
        }
    }

    virtual void HandleDisconnect();

    virtual void HandleWrite();
    virtual void FinishWrite(asio::error_code ErrorCode, std::size_t BytesTransferred);

    virtual void HandleRead();
    virtual void FinishRead(asio::error_code ErrorCode, std::size_t BytesTransferred);

    // Process received data
    virtual void OnRead(const uint8_t* Data, size_t Size) = 0;

    // State management
    virtual void SetActive(bool ActiveStatus);

    virtual void OnDisconnect() = 0;

    // Error handling
    static bool IsFatalError(const asio::error_code& ErrorCode);

public:
    Strand<ExecutorType> m_Strand;
    TcpSocket m_Socket;
    uint64_t m_Id;
    bool m_IsActive;
    std::deque<IPacketBasePtr> m_WriteQueue;
    asio::streambuf m_ReadBuffer;
    bool m_IsWriting;
};

} // namespace DrowsyNetwork