#pragma once

#include <asio.hpp>

namespace DrowsyNetwork {

    /**
     * @brief Core type aliases for ASIO networking components
     *
     * These aliases provide a consistent interface while hiding ASIO implementation
     * details. If you ever need to switch from standalone ASIO to Boost.ASIO,
     * you only need to change these definitions.
     */

    /// The main I/O execution context - think of it as your networking event loop
    using Executor = asio::io_context;

    /// Executor type for strand operations - ensures thread safety
    using ExecutorType = Executor::executor_type;

    /// TCP socket for client connections - your main communication channel
    using TcpSocket = asio::ip::tcp::socket;

    /// TCP acceptor for servers - listens for incoming connections
    using TcpAcceptor = asio::ip::tcp::acceptor;

    /// Resolves hostnames to IP addresses - handles "localhost" -> "127.0.0.1"
    using TcpResolver = asio::ip::tcp::resolver;

    /// Represents an IP:port combination
    using TcpEndpoint = asio::ip::tcp::endpoint;

    /**
     * @brief Strand wrapper for serializing operations
     * @tparam T Executor type
     *
     * Strands ensure that your async operations run one at a time, even in
     * multi-threaded environments. Think of it as a queue that processes
     * one thing at a time.
     */
    template<typename T>
    using Strand = asio::strand<T>;

    /// Mutable buffer for writing data - points to memory you can modify
    using Buffer = asio::mutable_buffer;

    /// Immutable buffer for reading data - points to read-only memory
    using ConstBuffer = asio::const_buffer;

    /// Standardized size type for all size operations
    /// Using int64_t instead of size_t to avoid signed/unsigned comparison issues
    using SizeType = int64_t;

} // namespace DrowsyNetwork