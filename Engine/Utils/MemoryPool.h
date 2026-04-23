#pragma once

#include <cstddef>
#include <mutex>

namespace Engine {

class FixedBlockMemoryPool {
public:
    struct Stats {
        std::size_t blockSize = 0;
        std::size_t blockCount = 0;
        std::size_t freeBlockCount = 0;
        std::size_t inUseBlockCount = 0;
        std::size_t peakInUseBlockCount = 0;
        std::size_t fallbackAllocationCount = 0;
        std::size_t totalAllocationCount = 0;
    };

public:
    FixedBlockMemoryPool(std::size_t blockSize, std::size_t blockCount);
    ~FixedBlockMemoryPool();

    FixedBlockMemoryPool(const FixedBlockMemoryPool&) = delete;
    FixedBlockMemoryPool& operator=(const FixedBlockMemoryPool&) = delete;

    void* Allocate(std::size_t size);
    void Deallocate(void* ptr, std::size_t size) noexcept;

    Stats GetStats() const;

private:
    struct FreeNode {
        FreeNode* next = nullptr;
    };

private:
    bool OwnsPointer(const void* ptr) const noexcept;

private:
    mutable std::mutex m_Mutex;
    std::byte* m_BufferBegin = nullptr;
    std::byte* m_BufferEnd = nullptr;
    FreeNode* m_FreeList = nullptr;
    std::size_t m_BlockSize = 0;
    std::size_t m_BlockCount = 0;
    std::size_t m_FreeBlockCount = 0;
    std::size_t m_InUseBlockCount = 0;
    std::size_t m_PeakInUseBlockCount = 0;
    std::size_t m_FallbackAllocationCount = 0;
    std::size_t m_TotalAllocationCount = 0;
};

FixedBlockMemoryPool& GetMeshMemoryPool();

} // namespace Engine

