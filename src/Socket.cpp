#include "drowsynetwork/Socket.hpp"
#include <print>

namespace DrowsyNetwork {

Socket::Socket(Executor& io_context, TcpSocket&& socket) :
    m_Strand(io_context.get_executor()),
    m_Socket(std::move(socket)) {
    std::println("Socket created");
}

TcpSocket& Socket::GetSocket() {
    return m_Socket;
}

// It is the duty of the developer to guarantee that the data
// remains valid until writing ends.

void Socket::Send(const uint8_t* data, size_t size) {
    auto DataSpan = std::span<const uint8_t>(data, size);
    asio::post(m_Strand, [this, DataSpan = DataSpan]() mutable {
        bool IsSending = !m_WriteQueue.empty();

        std::println("Sending {} bytes", DataSpan.size());
        m_WriteQueue.push_back(DataSpan);

        if (!IsSending)
            DoWrite();
    });
}

void Socket::DoWrite() {
    if (m_WriteQueue.empty())
        return;

    auto& DataSpan = m_WriteQueue.front();

    asio::async_write(m_Socket, asio::buffer(DataSpan),
        asio::bind_executor(m_Strand, [this](ErrorCode error, std::size_t bytes_transferred) mutable {
            if (error) {
                std::println("Error: {}", error.message());
                return;
            }

            std::println("Sent {} bytes", bytes_transferred);
            m_WriteQueue.pop_front();
            if (!m_WriteQueue.empty())
                DoWrite();

    }));
}

} // namespace DrowsyNetwork