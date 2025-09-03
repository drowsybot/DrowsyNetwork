#pragma once

namespace DrowsyNetwork {

class IPacketBase {
public:
    IPacketBase() = default;
    virtual ~IPacketBase() noexcept = default;

    [[nodiscard]] virtual size_t size() const noexcept = 0;
    [[nodiscard]] virtual const uint8_t* data() const noexcept = 0;
};

template <typename T>
concept PacketConcept = requires(std::remove_reference_t<T> t) {
        { t.size() } -> std::convertible_to<size_t>;
        { t.data() } -> std::convertible_to<const char*>;
    } || requires(std::remove_reference_t<T> t) {
        { t.GetSize() } -> std::convertible_to<size_t>;
        { t.GetBufferPointer() } -> std::convertible_to<const uint8_t*>;
};

template<PacketConcept T>
class PacketBase : public IPacketBase {
public:
    PacketBase() noexcept = default;

    template<typename... Args>
    explicit PacketBase(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : m_Data(std::forward<Args>(args)...) {}

    ~PacketBase() noexcept override = default;

    T* get() noexcept { return &m_Data; }
    const T* get() const noexcept { return &m_Data; }

    [[nodiscard]] size_t size() const noexcept override {
        if constexpr (requires { m_Data.size(); }) {
            return m_Data.size();
        } else if constexpr (requires { m_Data.GetSize(); }) {
            return m_Data.GetSize();
        } else {
            static_assert(sizeof(T) == 0, "Type T must have either size() or GetSize() method");
        }
    }

    [[nodiscard]] const uint8_t* data() const noexcept override {
        if constexpr (requires { m_Data.data(); }) {
            return reinterpret_cast<const uint8_t*>(m_Data.data());
        } else if constexpr (requires { m_Data.GetBufferPointer(); }) {
            return m_Data.GetBufferPointer();
        } else {
            static_assert(sizeof(T) == 0, "Type T must have either data() or GetData() method");
        }
    }

    template<typename... Args>
    static std::shared_ptr<PacketBase<T>> Create(Args&&... args) {
        return std::make_shared<PacketBase<T>>(std::forward<Args>(args)...);
    }

private:
    T m_Data;
};

// Type alias for cleaner usage
template<PacketConcept T>
using PacketPtr = std::shared_ptr<PacketBase<T>>;
} // namespace DrowsyNetwork