#include "NovelMind/vfs/file_handle.hpp"
#include <algorithm>
#include <cstring>

namespace NovelMind::VFS {

Result<std::vector<u8>> IFileHandle::readAll() {
  if (!isValid()) {
    return Result<std::vector<u8>>::error("Invalid file handle");
  }

  auto seekResult = seek(0, SeekOrigin::Begin);
  if (!seekResult.isOk()) {
    return Result<std::vector<u8>>::error(seekResult.error());
  }

  const usize fileSize = size();
  std::vector<u8> buffer(fileSize);

  auto readResult = read(buffer.data(), fileSize);
  if (!readResult.isOk()) {
    return Result<std::vector<u8>>::error(readResult.error());
  }

  buffer.resize(readResult.value());
  return Result<std::vector<u8>>::ok(std::move(buffer));
}

Result<std::vector<u8>> IFileHandle::readBytes(usize count) {
  if (!isValid()) {
    return Result<std::vector<u8>>::error("Invalid file handle");
  }

  std::vector<u8> buffer(count);
  auto readResult = read(buffer.data(), count);

  if (!readResult.isOk()) {
    return Result<std::vector<u8>>::error(readResult.error());
  }

  buffer.resize(readResult.value());
  return Result<std::vector<u8>>::ok(std::move(buffer));
}

MemoryFileHandle::MemoryFileHandle(std::vector<u8> data)
    : m_data(std::move(data)), m_position(0), m_valid(true) {}

MemoryFileHandle::MemoryFileHandle(const u8* data, usize dataSize)
    : m_data(data, data + dataSize), m_position(0), m_valid(true) {}

bool MemoryFileHandle::isValid() const {
  return m_valid;
}

usize MemoryFileHandle::size() const {
  return m_data.size();
}

usize MemoryFileHandle::position() const {
  return m_position;
}

bool MemoryFileHandle::isEof() const {
  return m_position >= m_data.size();
}

Result<usize> MemoryFileHandle::read(u8* buffer, usize count) {
  if (!m_valid) {
    return Result<usize>::error("Invalid file handle");
  }

  if (buffer == nullptr) {
    return Result<usize>::error("Null buffer");
  }

  const usize available = m_data.size() - m_position;
  const usize toRead = std::min(count, available);

  if (toRead > 0) {
    std::memcpy(buffer, m_data.data() + m_position, toRead);
    m_position += toRead;
  }

  return Result<usize>::ok(toRead);
}

Result<void> MemoryFileHandle::seek(i64 offset, SeekOrigin origin) {
  if (!m_valid) {
    return Result<void>::error("Invalid file handle");
  }

  i64 newPosition = 0;

  switch (origin) {
  case SeekOrigin::Begin:
    newPosition = offset;
    break;
  case SeekOrigin::Current:
    newPosition = static_cast<i64>(m_position) + offset;
    break;
  case SeekOrigin::End:
    newPosition = static_cast<i64>(m_data.size()) + offset;
    break;
  }

  if (newPosition < 0) {
    return Result<void>::error("Seek position before beginning of file");
  }

  if (static_cast<usize>(newPosition) > m_data.size()) {
    return Result<void>::error("Seek position past end of file");
  }

  m_position = static_cast<usize>(newPosition);
  return Result<void>::ok();
}

} // namespace NovelMind::VFS
