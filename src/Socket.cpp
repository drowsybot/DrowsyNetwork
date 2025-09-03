#include "drowsynetwork/Socket.hpp"

namespace DrowsyNetwork {

std::atomic<uint64_t> Socket::s_NextId{1};

Socket::Socket(Executor& io_context, TcpSocket&& socket) :
    m_Id(s_NextId.fetch_add(1)),
    m_Strand(io_context.get_executor()),
    m_Socket(std::move(socket)),
    m_IsWriting(false),
    m_IsActive(false) {
    LOG_DEBUG("Socket {} created", m_Id);
}

TcpSocket& Socket::GetSocket() {
    return m_Socket;
}

void Socket::Setup() {
    asio::post(m_Strand, [self = weak_from_this()]() {
        if (auto socket = self.lock()) {
            socket->SetActiveStatus(true);
            socket->HandleRead(); // Start reading process...
        }
    });
}

void Socket::HandleWrite() {
    if (!IsActive() || m_WriteQueue.empty())
        return;

    auto& instance = m_WriteQueue.front();

    asio::async_write(m_Socket, asio::buffer(instance.Packet->data(), instance.Packet->size()),
        asio::bind_executor(m_Strand, [self = weak_from_this()](asio::error_code error, std::size_t bytes_transferred) mutable {
            if (auto socket = self.lock()) {
                socket->FinishWrite(error, bytes_transferred);
            } else {
                // Handle invalid socket
                LOG_ERROR("Invalid socket at handle write");
            }
    }));
}

void Socket::FinishWrite(asio::error_code error, std::size_t bytes_transferred) {
    if (!IsActive())
        return;

    if (error) {
        LOG_ERROR("Socket {} write failed: {}", m_Id, error.message());
        if (IsFatalError(error)) {
            SetActiveStatus(false);
            return;
        }
    }

    auto& instance = m_WriteQueue.front();
    LOG_DEBUG("Socket {} sent {} bytes, remaining {} ref count", m_Id, instance.Size, instance.Packet.use_count());
    m_WriteQueue.pop_front();
    if (!m_WriteQueue.empty())
        HandleWrite();
    else
        m_IsWriting = false;
}

void Socket::HandleRead() {
    asio::async_read(m_Socket, m_ReadBuffer, asio::transfer_at_least(1),
        asio::bind_executor(m_Strand,
        [self = weak_from_this()](asio::error_code error, std::size_t bytes_transferred) {
            if (auto socket = self.lock()) {
                socket->FinishRead(error, bytes_transferred);
            }
        }
    ));
}

void Socket::FinishRead(asio::error_code error, std::size_t bytes_transferred) {
    if (!IsActive())
        return;

    if (error) {
        LOG_ERROR("Socket {} read failed: {}", m_Id, error.message());
        if (IsFatalError(error)) {
            SetActiveStatus(false);
            return;
        }
    }

    const auto Data = m_ReadBuffer.data();

    OnRead(static_cast<const uint8_t*>(Data.data()), Data.size());

    m_ReadBuffer.consume(bytes_transferred);
    HandleRead();
}

void Socket::SetActiveStatus(bool ActiveStatus) {
    m_IsActive = ActiveStatus;
    if (!IsActive()) {
        m_WriteQueue.clear();
        m_Socket.close();
        LOG_INFO("Socket {} set to inactive and closed", m_Id);
    }
}

bool Socket::IsActive() const {
    return m_IsActive;
}

bool Socket::IsFatalError(const asio::error_code& errorCode) {
    return errorCode == asio::error::eof ||
           errorCode == asio::error::connection_reset ||
           errorCode == asio::error::connection_aborted ||
           errorCode == asio::error::network_down ||
           errorCode == asio::error::network_unreachable ||
           errorCode == asio::error::timed_out ||
           errorCode == asio::error::broken_pipe;
}

} // namespace DrowsyNetwork