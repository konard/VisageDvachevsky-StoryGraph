#pragma once

#include <cstdlib>
#include <iostream>

namespace NovelMind::core {

inline void assertFailed(const char* condition, const char* message, const char* file, int line) {
  std::cerr << "Assertion failed: " << condition << "\n"
            << "Message: " << message << "\n"
            << "File: " << file << "\n"
            << "Line: " << line << std::endl;
  std::abort();
}

} // namespace NovelMind::core

#ifdef NDEBUG
#define NOVELMIND_ASSERT(condition, message) ((void)0)
#else
#define NOVELMIND_ASSERT(condition, message)                                                       \
  do {                                                                                             \
    if (!(condition)) {                                                                            \
      NovelMind::core::assertFailed(#condition, message, __FILE__, __LINE__);                      \
    }                                                                                              \
  } while (false)
#endif

#define NOVELMIND_ASSERT_NOT_NULL(ptr)                                                             \
  NOVELMIND_ASSERT((ptr) != nullptr, "Pointer must not be null")
