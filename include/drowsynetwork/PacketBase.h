#pragma once

#include <atomic>
#include <memory>
#include <utility>

namespace DrowsyNetwork {

template<typename T>
class PacketBase {
public:
    // Default constructor - creates empty packet
    PacketBase() : m_Data(nullptr), m_RefCount(0) {}

    // Constructor that takes ownership of data
    explicit PacketBase(T* data) : m_Data(data), m_RefCount(1) {}

    // Constructor that creates data in-place
    template<typename... Args>
    explicit PacketBase(Args&&... args)
        : m_Data(new T(std::forward<Args>(args)...)), m_RefCount(1) {}

    // Copy constructor - increment reference count
    PacketBase(const PacketBase& other) : m_Data(other.m_Data), m_RefCount(0) {
        if (m_Data) {
            // Atomically increment the shared reference count
            other.m_RefCount.fetch_add(1, std::memory_order_relaxed);
            m_RefCount.store(1, std::memory_order_relaxed);
        }
    }

    // Move constructor - transfer ownership
    PacketBase(PacketBase&& other) noexcept
        : m_Data(other.m_Data), m_RefCount(other.m_RefCount.load(std::memory_order_relaxed)) {
        other.m_Data = nullptr;
        other.m_RefCount.store(0, std::memory_order_relaxed);
    }

    // Copy assignment
    PacketBase& operator=(const PacketBase& other) {
        if (this != &other) {
            // Release current data
            Release();

            // Copy new data
            m_Data = other.m_Data;
            if (m_Data) {
                other.m_RefCount.fetch_add(1, std::memory_order_relaxed);
                m_RefCount.store(1, std::memory_order_relaxed);
            } else {
                m_RefCount.store(0, std::memory_order_relaxed);
            }
        }
        return *this;
    }

    // Move assignment
    PacketBase& operator=(PacketBase&& other) noexcept {
        if (this != &other) {
            // Release current data
            Release();

            // Transfer ownership
            m_Data = other.m_Data;
            m_RefCount.store(other.m_RefCount.load(std::memory_order_relaxed), std::memory_order_relaxed);

            other.m_Data = nullptr;
            other.m_RefCount.store(0, std::memory_order_relaxed);
        }
        return *this;
    }

    // Destructor
    ~PacketBase() {
        Release();
    }

    // Access the data
    T* Get() const { return m_Data; }
    T& operator*() const { return *m_Data; }
    T* operator->() const { return m_Data; }

    // Check if packet has data
    bool IsValid() const { return m_Data != nullptr; }
    explicit operator bool() const { return IsValid(); }

    // Get current reference count (mainly for debugging)
    size_t GetRefCount() const {
        return m_Data ? m_RefCount.load(std::memory_order_relaxed) : 0;
    }

    // Reset the packet (release current data)
    void Reset() {
        Release();
        m_Data = nullptr;
        m_RefCount.store(0, std::memory_order_relaxed);
    }

    // Create a new packet with data
    template<typename... Args>
    static PacketBase Create(Args&&... args) {
        return PacketBase(std::forward<Args>(args)...);
    }

    // Create packet from existing data pointer
    static PacketBase FromPointer(T* data) {
        return PacketBase(data);
    }

private:
    void Release() {
        if (m_Data) {
            // Decrement reference count
            size_t oldCount = m_RefCount.fetch_sub(1, std::memory_order_acq_rel);

            // If this was the last reference, delete the data
            if (oldCount == 1) {
                delete m_Data;
            }
        }
    }

private:
    T* m_Data;
    mutable std::atomic<size_t> m_RefCount;
};

// Convenience type aliases for common packet types
using BytePacket = PacketBase<std::vector<uint8_t>>;
using StringPacket = PacketBase<std::string>;

// Helper function to create packets
template<typename T, typename... Args>
PacketBase<T> MakePacket(Args&&... args) {
    return PacketBase<T>::Create(std::forward<Args>(args)...);
}

} // namespace DrowsyNetwork