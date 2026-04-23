#include "MemoryPool.h"

#include <algorithm>
#include <new>

namespace Engine {

namespace {

std::size_t AlignUp(std::size_t value, std::size_t alignment) {
    const std::size_t mask = alignment - 1;
    return (value + mask) & ~mask;
}

} // namespace

FixedBlockMemoryPool::FixedBlockMemoryPool(std::size_t blockSize, std::size_t blockCount) {
    const std::size_t minBlockSize = std::max(blockSize, sizeof(FreeNode));
    const std::size_t alignment = alignof(std::max_align_t);
    m_BlockSize = AlignUp(minBlockSize, alignment);
    m_BlockCount = blockCount;
    m_FreeBlockCount = blockCount;

    const std::size_t totalBytes = m_BlockSize * m_BlockCount;
    m_BufferBegin = static_cast<std::byte*>(::operator new(totalBytes, std::align_val_t(alignment)));
    m_BufferEnd = m_BufferBegin + totalBytes;

    for (std::size_t i = 0; i < m_BlockCount; ++i) {
        auto* node = reinterpret_cast<FreeNode*>(m_BufferBegin + i * m_BlockSize);
        node->next = m_FreeList;
        m_FreeList = node;
    }
}

FixedBlockMemoryPool::~FixedBlockMemoryPool() {
    if (m_BufferBegin != nullptr) {
        ::operator delete(m_BufferBegin, std::align_val_t(alignof(std::max_align_t)));
        m_BufferBegin = nullptr;
        m_BufferEnd = nullptr;
    }
}

void* FixedBlockMemoryPool::Allocate(std::size_t size) {
    if (size == 0) {
        size = 1;
    }

    std::lock_guard<std::mutex> guard(m_Mutex);
    ++m_TotalAllocationCount;

    if (size <= m_BlockSize && m_FreeList != nullptr) {
        FreeNode* node = m_FreeList;
        m_FreeList = node->next;
        --m_FreeBlockCount;
        ++m_InUseBlockCount;
        m_PeakInUseBlockCount = std::max(m_PeakInUseBlockCount, m_InUseBlockCount);
        return node;
    }

    ++m_FallbackAllocationCount;
    return ::operator new(size);
}

void FixedBlockMemoryPool::Deallocate(void* ptr, std::size_t size) noexcept {
    (void)size;

    if (ptr == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> guard(m_Mutex);
    if (OwnsPointer(ptr)) {
        auto* node = static_cast<FreeNode*>(ptr);
        node->next = m_FreeList;
        m_FreeList = node;
        ++m_FreeBlockCount;
        if (m_InUseBlockCount > 0) {
            --m_InUseBlockCount;
        }
        return;
    }

    ::operator delete(ptr);
}

FixedBlockMemoryPool::Stats FixedBlockMemoryPool::GetStats() const {
    std::lock_guard<std::mutex> guard(m_Mutex);
    Stats stats{};
    stats.blockSize = m_BlockSize;
    stats.blockCount = m_BlockCount;
    stats.freeBlockCount = m_FreeBlockCount;
    stats.inUseBlockCount = m_InUseBlockCount;
    stats.peakInUseBlockCount = m_PeakInUseBlockCount;
    stats.fallbackAllocationCount = m_FallbackAllocationCount;
    stats.totalAllocationCount = m_TotalAllocationCount;
    return stats;
}

bool FixedBlockMemoryPool::OwnsPointer(const void* ptr) const noexcept {
    const auto* bytePtr = static_cast<const std::byte*>(ptr);
    if (bytePtr < m_BufferBegin || bytePtr >= m_BufferEnd) {
        return false;
    }

    const std::size_t offset = static_cast<std::size_t>(bytePtr - m_BufferBegin);
    return (offset % m_BlockSize) == 0;
}

FixedBlockMemoryPool& GetMeshMemoryPool() {
    static FixedBlockMemoryPool pool(64 * 1024, 128);
    return pool;
}

} // namespace Engine

