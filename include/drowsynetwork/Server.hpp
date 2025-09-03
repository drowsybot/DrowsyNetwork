#pragma once

#include "Common.hpp"

namespace DrowsyNetwork {

/**
 * @brief Base class for TCP servers
 *
 * This class handles the low-level details of binding to ports, listening
 * for connections, and accepting clients. You inherit from this class and
 * override OnAccept() to handle new connections.
 *
 * The server automatically supports both IPv4 and IPv6, and can bind to
 * multiple addresses simultaneously (useful for dual-stack setups).
 *
 * Example usage:
 * @code
 * class MyServer : public DrowsyNetwork::Server {
 * public:
 *     MyServer(asio::io_context& io) : Server(io) {}
 *
 * protected:
 *     void OnAccept(TcpSocket&& socket) override {
 *         auto client = std::make_shared<MySocket>(m_IoContext, std::move(socket));
 *         client->Setup();
 *         m_clients.push_back(client);
 *     }
 * };
 * @endcode
 */
class Server {
public:
    Server() = delete;

    /**
     * @brief Construct server with an I/O context
     * @param IoContext The ASIO I/O context to use for all operations
     *
     * The I/O context must remain alive for the lifetime of the server.
     * All async operations will be executed on this context's thread(s).
     */
    explicit Server(Executor& IoContext);

    /**
     * @brief Virtual destructor - properly closes all acceptors
     *
     * Automatically closes any open acceptors and cleans up resources.
     * It's safe to destroy the server even if it's currently listening.
     */
    virtual ~Server();

    /**
     * @brief Bind to a host and port combination
     * @param Host Hostname or IP address (e.g., "localhost", "0.0.0.0", "::1")
     * @param Port Port number or service name (e.g., "8080", "http")
     * @return true if bound to at least one address, false if all failed
     *
     * This method resolves the hostname and attempts to bind to all resulting
     * addresses. For example, binding to "localhost" might create acceptors
     * for both 127.0.0.1 and ::1 (IPv6 loopback).
     *
     * Common patterns:
     * - Bind("0.0.0.0", "8080") - Listen on all IPv4 interfaces
     * - Bind("::", "8080") - Listen on all IPv6 interfaces
     * - Bind("localhost", "8080") - Listen on loopback only
     */
    bool Bind(std::string_view Host, std::string_view Port);

    /**
     * @brief Bind to a specific endpoint
     * @param Endpoint Pre-constructed TCP endpoint
     * @return true if successfully bound, false otherwise
     *
     * Use this when you need precise control over the binding address,
     * or when you've already resolved the address elsewhere.
     *
     * @code
     * asio::ip::tcp::endpoint ep(asio::ip::address_v4::any(), 8080);
     * server.Bind(ep);
     * @endcode
     */
    bool Bind(const TcpEndpoint& Endpoint);

    /**
     * @brief Start listening for connections on all bound addresses
     *
     * Call this after binding to begin accepting connections. The server
     * will continuously accept new connections until destroyed.
     *
     * This method doesn't block - connections are handled asynchronously.
     * Make sure to call io_context.run() to actually process events.
     */
    void StartListening();

    /**
     * @brief Get a specific acceptor by index
     * @param Index Zero-based acceptor index
     * @return Pointer to acceptor, or nullptr if index is invalid
     *
     * Mainly useful for advanced scenarios where you need direct access
     * to the underlying acceptors, like setting custom socket options.
     */
    [[nodiscard]] TcpAcceptor* GetAcceptor(size_t Index);

protected:
    /**
     * @brief Create a new acceptor for the given protocol
     * @param Protocol TCP protocol (IPv4 or IPv6)
     * @return Pointer to the new acceptor, or nullptr if creation failed
     *
     * This method creates and configures a new acceptor with sensible defaults:
     * - Reuse address option enabled
     * - IPv6-only flag set for IPv6 acceptors
     */
    [[nodiscard]] TcpAcceptor* CreateAcceptor(const asio::ip::tcp& Protocol);

    /**
     * @brief Start async accept operation for a specific acceptor
     * @param Index Acceptor index to start listening on
     *
     * This begins the async accept loop for the specified acceptor.
     * When a connection arrives, Accept() will be called.
     */
    void Listen(size_t Index);

    /**
     * @brief Handle an accepted connection
     * @param Index Index of the acceptor that accepted the connection
     * @param Socket The new client socket (moved)
     * @param ErrorCode Any error that occurred during accept
     *
     * This method logs the connection and calls your OnAccept() handler.
     * After handling the connection, it automatically starts listening
     * for the next connection.
     */
    void Accept(size_t Index, TcpSocket&& Socket, asio::error_code ErrorCode);

    /**
     * @brief Safely close an acceptor
     * @param Acceptor Reference to the acceptor to close
     *
     * Cancels pending operations and closes the acceptor socket.
     * Handles errors gracefully and logs any issues.
     */
    void CloseAcceptor(TcpAcceptor& Acceptor);

    /**
     * @brief Override this to handle new client connections
     * @param Socket Newly connected client socket
     *
     * This is where you create your Socket-derived objects and set them up.
     * The socket is already connected and ready to use.
     *
     * Example:
     * @code
     * void OnAccept(TcpSocket&& socket) override {
     *     auto client = std::make_shared<MySocket>(m_IoContext, std::move(socket));
     *     client->Setup();  // Important! This starts the read loop
     * }
     * @endcode
     */
    virtual void OnAccept(TcpSocket&& Socket) = 0;

protected:
    Executor& m_IoContext;           ///< Reference to the I/O context
    std::vector<TcpAcceptor> m_Acceptors; ///< All bound acceptors
    TcpResolver m_Resolver;          ///< For hostname resolution
};

} // namespace DrowsyNetwork