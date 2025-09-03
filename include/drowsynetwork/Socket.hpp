#pragma once

#include "Common.hpp"
#include "PacketBase.hpp"
#include "Logging.hpp"
#include <queue>
#include <memory>
#include <span>
#include <atomic>

namespace DrowsyNetwork {
/**
 * @brief Represents a single TCP connection
 *
 * This class wraps an ASIO TCP socket and provides a high-level interface
 * for sending/receiving data. It handles all the async complexity, thread
 * safety, and error management for you.
 *
 * Key features:
 * - Thread-safe sending from any thread
 * - Automatic connection management
 * - Flexible packet system
 * - Built-in error handling and logging
 * - Reference counting for safe async operations
 *
 * To use this class:
 * 1. Inherit from Socket
 * 2. Override OnRead() to handle incoming data
 * 3. Override OnDisconnect() to handle connection loss
 * 4. Call Setup() after construction to start the read loop
 *
 * @code
 * class MySocket : public DrowsyNetwork::Socket {
 * public:
 *     using Socket::Socket;  // Inherit constructors
 *
 * protected:
 *     void OnRead(const uint8_t* data, size_t size) override {
 *         // Process incoming data
 *         std::string message(reinterpret_cast<const char*>(data), size);
 *         LOG_INFO("Received: {}", message);
 *     }
 *
 *     void OnDisconnect() override {
 *         LOG_INFO("Client {} disconnected", GetId());
 *     }
 * };
 * @endcode
 */
class Socket : public std::enable_shared_from_this<Socket> {
public:
    Socket() = delete;

    /**
     * @brief Construct socket with I/O context and connected socket
     * @param IOContext The ASIO I/O context for async operations
     * @param Socket Already connected TCP socket (moved)
     *
     * The socket should already be connected (usually from Server::OnAccept).
     * Each socket gets a unique ID for logging and identification.
     */
    explicit Socket(Executor& IOContext, TcpSocket&& Socket);

    /**
     * @brief Virtual destructor for proper cleanup
     *
     * Automatically handles disconnection and cleanup if the socket
     * is still active when destroyed.
     */
    virtual ~Socket() = default;

    /**
     * @brief Get reference to the underlying TCP socket
     * @return Reference to the ASIO socket
     *
     * Use this when you need direct access to socket options or
     * properties like remote endpoint address.
     */
    TcpSocket& GetSocket();

    /**
     * @brief Get unique socket identifier
     * @return Unique socket ID (never reused)
     *
     * Useful for logging, tracking, and associating sockets with
     * application-level data structures.
     */
    uint64_t GetId() const { return m_Id; }

    /**
     * @brief Send a packet to the remote peer (thread-safe)
     * @tparam T Packet data type
     * @param Packet Shared pointer to packet to send
     *
     * This method is fully thread-safe and can be called from any thread.
     * Packets are queued and sent in order. If called from the socket's
     * strand thread, the packet is queued directly. Otherwise, it's posted
     * to the strand for thread-safe handling.
     *
     * The packet will be kept alive until transmission is complete, so
     * it's safe to let your local copy go out of scope immediately.
     *
     * Example:
     * @code
     * auto packet = PacketBase<std::string>::Create("Hello World!");
     * socket->Send(packet);  // Safe from any thread
     * @endcode
     */
    template <PacketConcept T>
    void Send(const PacketPtr<T>& Packet) {
        if (m_Strand.running_in_this_thread()) {
            // Already on the correct thread - queue directly
            EnqueueSend(Packet);
        } else {
            // Post to strand for thread safety
            asio::post(m_Strand, [self = weak_from_this(), Packet = Packet]() {
                if (auto socket = self.lock()) {
                    socket->EnqueueSend(Packet);
                } else {
                    LOG_ERROR("Invalid socket at send");
                }
            });
        }
    }

    /**
     * @brief Initialize the socket and start reading (call after construction)
     *
     * This method must be called after creating a socket to begin the
     * async read loop. It's separate from the constructor to ensure
     * the derived class is fully constructed before async operations begin.
     *
     * Always call this in your Server::OnAccept implementation:
     * @code
     * void OnAccept(TcpSocket&& socket) override {
     *     auto client = std::make_shared<MySocket>(m_IoContext, std::move(socket));
     *     client->Setup();  // Don't forget this!
     * }
     * @endcode
     */
    virtual void Setup();

    /**
     * @brief Disconnect the socket gracefully
     *
     * Initiates graceful shutdown of the connection. This is thread-safe
     * and can be called multiple times safely. The socket will:
     * 1. Stop accepting new data
     * 2. Send any queued packets
     * 3. Close the connection
     * 4. Call OnDisconnect()
     */
    void Disconnect();

    /**
     * @brief Check if the socket is currently active
     * @return true if socket can send/receive data
     *
     * An active socket is one that's connected and hasn't encountered
     * a fatal error. Once a socket becomes inactive, it will remain
     * so until destroyed.
     */
    bool IsActive() const;

protected:
    /**
     * @brief Queue a packet for sending (internal, strand-only)
     * @tparam T Packet data type
     * @param Packet Shared pointer to packet to queue
     *
     * This is the internal implementation of Send(). It must only be
     * called from within the socket's strand thread for thread safety.
     * Use Send() from application code instead.
     */
    template <PacketConcept T>
    void EnqueueSend(const PacketPtr<T>& Packet) {
        if (!IsActive())
            return;

        m_WriteQueue.push_back(Packet);

        // Start writing if not already in progress
        if (!m_IsWriting) {
            m_IsWriting = true;
            HandleWrite();
        }
    }

    /**
     * @brief Handle disconnection cleanup (override for custom behavior)
     *
     * This method is called when the socket is being disconnected.
     * The default implementation handles socket closure and cleanup.
     * Override this if you need custom disconnection logic, but make
     * sure to call the base implementation.
     */
    virtual void HandleDisconnect();

    /**
     * @brief Start async write operation for queued packets
     *
     * Begins transmission of the first packet in the write queue.
     * This method ensures packets are sent in order and handles
     * the async write completion.
     */
    virtual void HandleWrite();

    /**
     * @brief Handle completion of async write operation
     * @param ErrorCode Any error that occurred during writing
     * @param BytesTransferred Number of bytes successfully sent
     *
     * This method is called when a packet has finished sending (or failed).
     * It removes the completed packet from the queue and starts sending
     * the next packet if any are queued.
     */
    virtual void FinishWrite(asio::error_code ErrorCode, std::size_t BytesTransferred);

    /**
     * @brief Start async read operation
     *
     * Begins reading data from the remote peer. This method sets up
     * an async read that will continue until some data arrives or
     * an error occurs.
     */
    virtual void HandleRead();

    /**
     * @brief Handle completion of async read operation
     * @param ErrorCode Any error that occurred during reading
     * @param BytesTransferred Number of bytes successfully read
     *
     * This method processes received data by calling OnRead(), then
     * continues the read loop. It handles connection errors and
     * manages the read buffer.
     */
    virtual void FinishRead(asio::error_code ErrorCode, std::size_t BytesTransferred);

    /**
     * @brief Process received data (override this in your derived class)
     * @param Data Pointer to received bytes
     * @param Size Number of bytes received
     *
     * This is where you implement your protocol logic. The data pointer
     * is valid only for the duration of this call. If you need to keep
     * the data beyond this method, make a copy.
     *
     * Example implementations:
     * @code
     * // Echo server
     * void OnRead(const uint8_t* data, size_t size) override {
     *     auto packet = PacketBase<std::vector<uint8_t>>::Create(data, data + size);
     *     Send(packet);
     * }
     *
     * // Line-based protocol
     * void OnRead(const uint8_t* data, size_t size) override {
     *     m_buffer.append(reinterpret_cast<const char*>(data), size);
     *
     *     size_t pos;
     *     while ((pos = m_buffer.find('\n')) != std::string::npos) {
     *         ProcessLine(m_buffer.substr(0, pos));
     *         m_buffer.erase(0, pos + 1);
     *     }
     * }
     * @endcode
     */
    virtual void OnRead(const uint8_t* Data, size_t Size) = 0;

    /**
     * @brief Set the socket's active status (internal use)
     * @param ActiveStatus New active status
     *
     * This method is used internally to manage the socket's state.
     * Don't call this directly - use Disconnect() instead.
     */
    virtual void SetActive(bool ActiveStatus);

    /**
     * @brief Handle socket disconnection (override this in your derived class)
     *
     * This method is called after the socket has been fully disconnected
     * and cleaned up. Use this for application-level cleanup like removing
     * the socket from collections, notifying other systems, etc.
     *
     * The socket is guaranteed to be inactive when this is called.
     *
     * Example:
     * @code
     * void OnDisconnect() override {
     *     LOG_INFO("Player {} left the game", GetId());
     *     m_gameServer->RemovePlayer(GetId());
     * }
     * @endcode
     */
    virtual void OnDisconnect() = 0;

    /**
     * @brief Check if an error code represents a fatal connection error
     * @param ErrorCode The error code to check
     * @return true if the error is fatal and requires disconnection
     *
     * This method distinguishes between temporary errors (which should
     * be retried) and fatal errors (which require disconnection).
     * Currently, most network errors are considered fatal for simplicity.
     */
    static bool IsFatalError(const asio::error_code& ErrorCode);

public:
    Strand<ExecutorType> m_Strand;      ///< Strand for thread-safe operations
    TcpSocket m_Socket;                 ///< The underlying ASIO socket
    uint64_t m_Id;                      ///< Unique socket identifier
    bool m_IsActive;                    ///< Current connection status
    std::deque<IPacketBasePtr> m_WriteQueue; ///< Outgoing packet queue
    asio::streambuf m_ReadBuffer;       ///< Buffer for incoming data
    bool m_IsWriting;                   ///< Flag to prevent overlapping writes
};
} // namespace DrowsyNetwork