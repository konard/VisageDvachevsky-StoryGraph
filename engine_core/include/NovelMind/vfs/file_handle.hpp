#pragma once

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include <cstddef>
#include <vector>

namespace NovelMind::VFS {

enum class SeekOrigin { Begin, Current, End };

class IFileHandle {
public:
  virtual ~IFileHandle() = default;

  [[nodiscard]] virtual bool isValid() const = 0;
  [[nodiscard]] virtual usize size() const = 0;
  [[nodiscard]] virtual usize position() const = 0;
  [[nodiscard]] virtual bool isEof() const = 0;

  virtual Result<usize> read(u8* buffer, usize count) = 0;
  virtual Result<void> seek(i64 offset, SeekOrigin origin = SeekOrigin::Begin) = 0;

  Result<std::vector<u8>> readAll();
  Result<std::vector<u8>> readBytes(usize count);
};

class MemoryFileHandle : public IFileHandle {
public:
  MemoryFileHandle() = default;
  explicit MemoryFileHandle(std::vector<u8> data);
  MemoryFileHandle(const u8* data, usize size);

  [[nodiscard]] bool isValid() const override;
  [[nodiscard]] usize size() const override;
  [[nodiscard]] usize position() const override;
  [[nodiscard]] bool isEof() const override;

  Result<usize> read(u8* buffer, usize count) override;
  Result<void> seek(i64 offset, SeekOrigin origin) override;

private:
  std::vector<u8> m_data;
  usize m_position = 0;
  bool m_valid = false;
};

} // namespace NovelMind::VFS
