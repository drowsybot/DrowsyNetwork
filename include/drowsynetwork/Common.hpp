#pragma once

#include <asio.hpp>

namespace DrowsyNetwork {

// Type aliases for common ASIO types
using Executor = asio::io_context;
using ExecutorType = Executor::executor_type;
using TcpSocket = asio::ip::tcp::socket;
using TcpAcceptor = asio::ip::tcp::acceptor;
using TcpEndpoint = asio::ip::tcp::endpoint;
using ErrorCode = asio::error_code;

template<typename T>
using Strand = asio::strand<T>;

// Buffer type alias
using Buffer = asio::mutable_buffer;
using ConstBuffer = asio::const_buffer;

} // namespace DrowsyNetwork