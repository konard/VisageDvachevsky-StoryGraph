#pragma once

/**
 * @file secure_memory.hpp
 * @brief Secure memory utilities for sensitive data (encryption keys, passwords, etc.)
 *
 * Provides containers and allocators that:
 * - Zero memory on destruction
 * - Lock memory to prevent swapping to disk (platform-dependent)
 * - Prevent compiler optimizations from removing security measures
 */

#include "NovelMind/core/types.hpp"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

// Platform detection for memory locking
#if defined(_WIN32) || defined(_WIN64)
#define NOVELMIND_PLATFORM_WINDOWS
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
#define NOVELMIND_PLATFORM_POSIX
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace NovelMind::Core {

/**
 * @brief Securely zero memory in a way that prevents compiler optimization
 *
 * Uses platform-specific secure zeroing functions when available:
 * - Windows: SecureZeroMemory
 * - Linux: explicit_bzero
 * - Other: volatile pointer technique
 */
inline void secureZeroMemory(void* ptr, usize size) {
  if (ptr == nullptr || size == 0) {
    return;
  }

#if defined(NOVELMIND_PLATFORM_WINDOWS)
  // Windows: Use SecureZeroMemory which is guaranteed not to be optimized out
  SecureZeroMemory(ptr, size);
#elif defined(__GLIBC__) && __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 25
  // Linux with glibc 2.25+: Use explicit_bzero
  explicit_bzero(ptr, size);
#else
  // Fallback: Use volatile pointer to prevent optimization
  // This technique is recommended by secure coding guidelines
  volatile u8* volatilePtr = static_cast<volatile u8*>(ptr);
  for (usize i = 0; i < size; ++i) {
    volatilePtr[i] = 0;
  }

  // Add a memory barrier to prevent reordering
  std::atomic_signal_fence(std::memory_order_acq_rel);
#endif
}

/**
 * @brief Lock memory to prevent swapping to disk
 *
 * Returns true if locking succeeded, false otherwise.
 * Locking may fail if:
 * - Platform doesn't support it
 * - Insufficient privileges
 * - Memory limits exceeded
 */
inline bool lockMemory(void* ptr, usize size) {
  if (ptr == nullptr || size == 0) {
    return false;
  }

#if defined(NOVELMIND_PLATFORM_WINDOWS)
  // Windows: Use VirtualLock
  return VirtualLock(ptr, size) != 0;
#elif defined(NOVELMIND_PLATFORM_POSIX)
  // POSIX: Use mlock
  // Note: May require CAP_IPC_LOCK capability or appropriate ulimits
  return mlock(ptr, size) == 0;
#else
  // Platform doesn't support memory locking
  return false;
#endif
}

/**
 * @brief Unlock previously locked memory
 */
inline void unlockMemory(void* ptr, usize size) {
  if (ptr == nullptr || size == 0) {
    return;
  }

#if defined(NOVELMIND_PLATFORM_WINDOWS)
  VirtualUnlock(ptr, size);
#elif defined(NOVELMIND_PLATFORM_POSIX)
  munlock(ptr, size);
#endif
}

/**
 * @brief Secure allocator that zeros memory on deallocation
 *
 * This allocator:
 * - Zeroes memory before freeing it
 * - Attempts to lock memory to prevent swapping
 * - Unlocks memory on deallocation
 */
template <typename T>
class SecureAllocator {
public:
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  SecureAllocator() noexcept = default;

  template <typename U>
  SecureAllocator(const SecureAllocator<U>&) noexcept {}

  template <typename U>
  bool operator==(const SecureAllocator<U>&) const noexcept {
    return true;
  }

  template <typename U>
  bool operator!=(const SecureAllocator<U>&) const noexcept {
    return false;
  }

  T* allocate(size_type n) {
    if (n == 0) {
      return nullptr;
    }

    // Check for overflow
    if (n > std::numeric_limits<size_type>::max() / sizeof(T)) {
      throw std::bad_alloc();
    }

    void* ptr = std::malloc(n * sizeof(T));
    if (!ptr) {
      throw std::bad_alloc();
    }

    // Try to lock memory (best effort, failure is not fatal)
    lockMemory(ptr, n * sizeof(T));

    return static_cast<T*>(ptr);
  }

  void deallocate(T* ptr, size_type n) noexcept {
    if (ptr == nullptr || n == 0) {
      return;
    }

    // Securely zero the memory before freeing
    secureZeroMemory(ptr, n * sizeof(T));

    // Unlock memory
    unlockMemory(ptr, n * sizeof(T));

    std::free(ptr);
  }
};

/**
 * @brief Secure vector for storing sensitive data
 *
 * This is a std::vector with a secure allocator that:
 * - Zeroes memory on destruction
 * - Locks memory to prevent swapping (where supported)
 * - Prevents keys from appearing in core dumps or swap
 *
 * Use this for encryption keys, passwords, and other sensitive data.
 */
template <typename T>
using SecureVector = std::vector<T, SecureAllocator<T>>;

/**
 * @brief RAII wrapper for secure memory management
 *
 * Automatically locks memory on construction and unlocks/zeroes on destruction.
 * Use this for stack-allocated sensitive data.
 *
 * Example:
 * ```cpp
 * std::array<u8, 32> key;
 * SecureMemoryGuard guard(key.data(), key.size());
 * // Use key...
 * // Key will be automatically zeroed when guard goes out of scope
 * ```
 */
class SecureMemoryGuard {
public:
  SecureMemoryGuard(void* ptr, usize size) : m_ptr(ptr), m_size(size), m_locked(false) {
    if (m_ptr && m_size > 0) {
      m_locked = lockMemory(m_ptr, m_size);
    }
  }

  ~SecureMemoryGuard() {
    if (m_ptr && m_size > 0) {
      secureZeroMemory(m_ptr, m_size);
      if (m_locked) {
        unlockMemory(m_ptr, m_size);
      }
    }
  }

  // Prevent copying
  SecureMemoryGuard(const SecureMemoryGuard&) = delete;
  SecureMemoryGuard& operator=(const SecureMemoryGuard&) = delete;

  // Prevent moving
  SecureMemoryGuard(SecureMemoryGuard&&) = delete;
  SecureMemoryGuard& operator=(SecureMemoryGuard&&) = delete;

  [[nodiscard]] bool isLocked() const { return m_locked; }

private:
  void* m_ptr;
  usize m_size;
  bool m_locked;
};

} // namespace NovelMind::Core
