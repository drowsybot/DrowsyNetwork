#pragma once

namespace DrowsyNetwork {

/**
 * @brief Base interface for all packet types
 *
 * This interface allows different packet types to be stored in the same
 * containers and queues. Every packet must be able to report its size
 * and provide access to its raw data.
 */
class IPacketBase {
public:
    IPacketBase() = default;
    virtual ~IPacketBase() noexcept = default;

    /**
     * @brief Get the total size of the packet in bytes
     * @return Size in bytes
     *
     * This should return the complete size needed to transmit the packet,
     * including any headers or metadata.
     */
    [[nodiscard]] virtual size_t size() const noexcept = 0;

    /**
     * @brief Get pointer to the raw packet data
     * @return Pointer to packet bytes
     *
     * The returned pointer must remain valid until the packet is destroyed.
     * The data should be ready for network transmission.
     */
    [[nodiscard]] virtual const uint8_t* data() const noexcept = 0;
};

/**
 * @brief Concept defining what makes a valid packet type
 * @tparam T The type to check
 *
 * A packet type must provide either:
 * - size() and data() methods (standard containers like std::vector, std::string)
 * - GetSize() and GetBufferPointer() methods (custom packet classes)
 *
 * This flexible design lets you use anything from simple strings to complex
 * protobuf messages as packets.
 */
template <typename T>
concept PacketConcept = requires(std::remove_reference_t<T> t) {
        { t.size() } -> std::convertible_to<size_t>;
        { t.data() } -> std::convertible_to<const char*>;
    } || requires(std::remove_reference_t<T> t) {
        { t.size() } -> std::convertible_to<size_t>;
        { t.data() } -> std::convertible_to<const uint8_t*>;
    } || requires(std::remove_reference_t<T> t) {
        { t.GetSize() } -> std::convertible_to<size_t>;
        { t.GetBufferPointer() } -> std::convertible_to<const uint8_t*>;
};

/**
 * @brief Type-safe packet wrapper for any data type
 * @tparam T The underlying packet data type
 *
 * This class wraps your custom data types to make them work with the
 * DrowsyNetwork system. It automatically adapts different method names
 * and handles the type safety.
 *
 * Example usage:
 * @code
 * // Using with std::string
 * auto packet = PacketBase<std::string>::Create("Hello World!");
 *
 * // Using with std::vector
 * std::vector<uint8_t> data = {0x01, 0x02, 0x03};
 * auto packet = PacketBase<std::vector<uint8_t>>::Create(data);
 *
 * // Using with custom class
 * class MyPacket {
 *     size_t GetSize() const { return m_data.size(); }
 *     const uint8_t* GetBufferPointer() const { return m_data.data(); }
 * };
 * auto packet = PacketBase<MyPacket>::Create(args...);
 * @endcode
 */
template<PacketConcept T>
class PacketBase : public IPacketBase {
public:
    PacketBase() noexcept = default;

    /**
     * @brief Construct packet with forwarded arguments
     * @tparam Args Argument types for T's constructor
     * @param args Arguments to forward to T's constructor
     *
     * This perfect forwarding constructor allows you to construct the
     * underlying data type directly within the packet wrapper.
     */
    template<typename... Args>
    explicit PacketBase(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : m_Data(std::forward<Args>(args)...) {}

    ~PacketBase() noexcept override = default;

    /**
     * @brief Get mutable reference to the underlying data
     * @return Pointer to the wrapped data object
     *
     * Use this when you need to modify the packet data after creation.
     * Be careful - changes after the packet is queued for sending may
     * cause race conditions.
     */
    T* get() noexcept { return &m_Data; }

    /**
     * @brief Get const reference to the underlying data
     * @return Const pointer to the wrapped data object
     *
     * Safe to call from any thread - returns read-only access to the data.
     */
    const T* get() const noexcept { return &m_Data; }

    /**
     * @brief Get the packet size using the appropriate method
     * @return Size in bytes
     *
     * Automatically calls either size() or GetSize() depending on what
     * the underlying type provides. This abstraction lets you mix different
     * packet types in the same system.
     */
    [[nodiscard]] size_t size() const noexcept override {
        if constexpr (requires { m_Data.size(); }) {
            return m_Data.size();
        } else if constexpr (requires { m_Data.GetSize(); }) {
            return m_Data.GetSize();
        } else {
            static_assert(sizeof(T) == 0, "Type T must have either size() or GetSize() method");
        }
    }

    /**
     * @brief Get raw packet data using the appropriate method
     * @return Pointer to packet bytes
     *
     * Automatically calls either data() or GetBufferPointer() and handles
     * any necessary type casting to uint8_t*. The returned pointer is valid
     * until the packet is destroyed.
     */
    [[nodiscard]] const uint8_t* data() const noexcept override {
        if constexpr (requires { m_Data.data(); }) {
            return reinterpret_cast<const uint8_t*>(m_Data.data());
        } else if constexpr (requires { m_Data.GetBufferPointer(); }) {
            return m_Data.GetBufferPointer();
        } else {
            static_assert(sizeof(T) == 0, "Type T must have either data() or GetBufferPointer() method");
        }
    }

    /**
     * @brief Factory method for creating shared packet instances
     * @tparam Args Argument types for T's constructor
     * @param args Arguments to forward to T's constructor
     * @return Shared pointer to the new packet
     *
     * This is the preferred way to create packets since the networking
     * system uses shared ownership for memory safety in async operations.
     *
     * @code
     * auto packet = PacketBase<std::string>::Create("Hello!");
     * socket->Send(packet);
     * @endcode
     */
    template<typename... Args>
    static std::shared_ptr<PacketBase<T>> Create(Args&&... args) {
        return std::make_shared<PacketBase<T>>(std::forward<Args>(args)...);
    }

private:
    T m_Data; ///< The actual packet data
};

/// Convenient alias for shared pointers to the base interface
using IPacketBasePtr = std::shared_ptr<IPacketBase>;

/// Convenient alias for shared pointers to typed packets
template<PacketConcept T>
using PacketPtr = std::shared_ptr<PacketBase<T>>;

} // namespace DrowsyNetwork