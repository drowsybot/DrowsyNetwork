#pragma once

#include "Common.hpp"
#include "PacketBase.h"
#include "Logging.hpp"
#include <queue>
#include <memory>
#include <span>
#include <atomic>

namespace DrowsyNetwork {

struct QueuedPacket {
    std::shared_ptr<IPacketBase> Packet;
    SizeType Size;
};

class Socket : public std::enable_shared_from_this<Socket> {
private:
    static std::atomic<uint64_t> s_NextId;
    uint64_t m_Id;

public:
    Socket() = delete;
    explicit Socket(Executor& io_context, TcpSocket&& socket);
    virtual ~Socket() = default;

    uint64_t GetId() const { return m_Id; }

    TcpSocket& GetSocket();

    // Taking a const reference to avoid uselessly incrementing the ref count
    // if answering from within the strand thread
    template <PacketConcept T>
    void Send(const PacketPtr<T>& packet) {
        if (!IsActive())
            return;

        if (m_Strand.running_in_this_thread()) {
            EnqueueSend(packet);
        } else {
            asio::post(m_Strand, [self = weak_from_this(), packet = packet]() {
                if (auto socket = self.lock()) {
                    socket->EnqueueSend(packet);
                } else {
                    // Handle invalid socket
                    LOG_ERROR("Invalid socket at send");
                }
            });
        }
    }

    // To be called after accepting the connection
    virtual void Setup();

protected:
    template <PacketConcept T>
    void EnqueueSend(const PacketPtr<T>& packet) {
        if (!IsActive())
            return;

        QueuedPacket queuedInstance;
        queuedInstance.Packet = packet;
        queuedInstance.Size = packet->size();

        m_WriteQueue.push_back(queuedInstance);

        if (!m_IsWriting) {
            m_IsWriting = true;
            HandleWrite();
        }
    }

    virtual void HandleWrite();
    void FinishWrite(asio::error_code error, std::size_t bytes_transferred);

    virtual void HandleRead();
    virtual void FinishRead(asio::error_code error, std::size_t bytes_transferred);

    // Process received data
    virtual void OnRead(const uint8_t* data, size_t size) {};

    // State management
    void SetActiveStatus(bool ActiveStatus);
    bool IsActive() const;

    // Error handling
    static bool IsFatalError(const asio::error_code& errorCode);

public:
    Strand<ExecutorType> m_Strand;
    TcpSocket m_Socket;
    std::deque<QueuedPacket> m_WriteQueue;
    asio::streambuf m_ReadBuffer;
    bool m_IsWriting;
    bool m_IsActive;
};

} // namespace DrowsyNetwork