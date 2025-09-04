#include "drowsynetwork/Socket.hpp"

namespace DrowsyNetwork {

Socket::Socket(Executor& IOContext, std::unique_ptr<TcpSocket>&& Socket) :
    m_Strand(IOContext.get_executor()),
    m_Socket(std::move(Socket)),
    m_IsWriting(false),
    m_IsActive(false) {
    static std::atomic<uint64_t> s_NextId(1);
    m_Id = s_NextId.fetch_add(1);

    LOG_DEBUG("Socket {} created", m_Id);
}

TcpSocket* Socket::GetSocket() const {
    return m_Socket.get();
}

void Socket::Setup() {
    asio::post(m_Strand, [self = weak_from_this()]() {
        if (auto Socket = self.lock()) {
            Socket->SetActive(true);
            Socket->HandleRead();
        }
    });
}

void Socket::HandleWrite() {
    if (!IsActive() || m_WriteQueue.empty())
        return;

    auto& Instance = m_WriteQueue.front();

    asio::async_write(*m_Socket, asio::buffer(Instance->data(), Instance->size()),
        asio::bind_executor(m_Strand, [self = weak_from_this()](asio::error_code ErrorCode, std::size_t BytesTransferred) {
            if (auto Socket = self.lock()) {
                Socket->FinishWrite(ErrorCode, BytesTransferred);
            } else {
                // Handle invalid socket
                LOG_ERROR("Invalid socket at handle write");
            }
    }));
}

void Socket::FinishWrite(asio::error_code ErrorCode, std::size_t BytesTransferred) {
    if (!IsActive())
        return;

    if (ErrorCode) {
        LOG_ERROR("Socket {} write failed: {}", m_Id, ErrorCode.message());
        // For write errors, always consider them fatal and close the connection
        // Partial writes are handled by asio::async_write, so any error here is serious
        Disconnect();
        return;
    }

    auto& Instance = m_WriteQueue.front();
    LOG_DEBUG("Socket {} sent {} bytes, remaining {} ref count", m_Id, Instance->size(), Instance.use_count());
    m_WriteQueue.pop_front();
    if (!m_WriteQueue.empty())
        HandleWrite();
    else
        m_IsWriting = false;
}

void Socket::HandleRead() {
    asio::async_read(*m_Socket, m_ReadBuffer, asio::transfer_at_least(1),
        asio::bind_executor(m_Strand,
        [self = weak_from_this()](asio::error_code ErrorCode, std::size_t BytesTransferred) {
            if (auto socket = self.lock()) {
                socket->FinishRead(ErrorCode, BytesTransferred);
            }
        }
    ));
}

void Socket::FinishRead(asio::error_code ErrorCode, std::size_t BytesTransferred) {
    if (!IsActive())
        return;

    if (ErrorCode) {
        LOG_ERROR("Socket {} read failed: {}", m_Id, ErrorCode.message());
        if (IsFatalError(ErrorCode) && IsActive()) {
            Disconnect();
        } else if (IsActive()) {
            HandleRead();
        }
        return;
    }

    const auto Data = m_ReadBuffer.data();

    OnRead(static_cast<const uint8_t*>(Data.data()), Data.size());

    m_ReadBuffer.consume(BytesTransferred);
    HandleRead();
}

void Socket::SetActive(bool ActiveStatus) {
    m_IsActive = ActiveStatus;
}

void Socket::Disconnect() {
    asio::dispatch(m_Strand, [self = weak_from_this()]() {
        if (auto Socket = self.lock()) {
            Socket->HandleDisconnect();
        }
    });
}

void Socket::HandleDisconnect() {
    if (m_Socket->is_open()) {
        asio::error_code ErrorCode;
        m_Socket->shutdown(asio::socket_base::shutdown_both, ErrorCode);
        if (ErrorCode && ErrorCode != asio::error::not_connected) {
            LOG_ERROR("Socket {} disconnect (shutdown): {}", m_Id, ErrorCode.message());
        }

        m_Socket->close(ErrorCode);
        if (ErrorCode && ErrorCode != asio::error::not_connected) {
            LOG_ERROR("Socket {} disconnect (close): {}", m_Id, ErrorCode.message());
        }
    }

    SetActive(false);
    m_WriteQueue.clear(); // Clear message queue
    m_IsWriting = false;

    LOG_DEBUG("Socket {} disconnected", m_Id);

    OnDisconnect();
}

bool Socket::IsActive() const {
    return m_IsActive;
}

bool Socket::IsFatalError(const asio::error_code& ErrorCode) {
    return ErrorCode == asio::error::eof ||
           ErrorCode == asio::error::connection_reset ||
           ErrorCode == asio::error::connection_aborted ||
           ErrorCode == asio::error::network_down ||
           ErrorCode == asio::error::network_unreachable ||
           ErrorCode == asio::error::timed_out ||
           ErrorCode == asio::error::broken_pipe ||
           ErrorCode == asio::error::operation_aborted;
}

} // namespace DrowsyNetwork