#pragma once

#include <optional>
#include <string>
#include <utility>

namespace NovelMind {

template <typename T, typename E = std::string> class Result {
public:
  static Result ok(T value) {
    Result r;
    r.m_value = std::move(value);
    r.m_hasValue = true;
    return r;
  }

  static Result error(E err) {
    Result r;
    r.m_error = std::move(err);
    r.m_hasValue = false;
    return r;
  }

  [[nodiscard]] bool isOk() const { return m_hasValue; }

  [[nodiscard]] bool isError() const { return !m_hasValue; }

  explicit operator bool() const { return isOk(); }

  bool operator!() const { return isError(); }

  [[nodiscard]] T& value() & { return m_value.value(); }

  [[nodiscard]] const T& value() const& { return m_value.value(); }

  [[nodiscard]] T&& value() && { return std::move(m_value.value()); }

  [[nodiscard]] E& error() & { return m_error.value(); }

  [[nodiscard]] const E& error() const& { return m_error.value(); }

  [[nodiscard]] T valueOr(T defaultValue) const {
    if (m_hasValue) {
      return m_value.value();
    }
    return defaultValue;
  }

private:
  std::optional<T> m_value;
  std::optional<E> m_error;
  bool m_hasValue = false;
};

template <typename E> class Result<void, E> {
public:
  static Result ok() {
    Result r;
    r.m_error = std::nullopt;
    return r;
  }

  static Result error(E err) {
    Result r;
    r.m_error = std::move(err);
    return r;
  }

  [[nodiscard]] bool isOk() const { return !m_error.has_value(); }

  [[nodiscard]] bool isError() const { return m_error.has_value(); }

  explicit operator bool() const { return isOk(); }

  bool operator!() const { return isError(); }

  [[nodiscard]] E& error() & { return m_error.value(); }

  [[nodiscard]] const E& error() const& { return m_error.value(); }

private:
  std::optional<E> m_error;
};

} // namespace NovelMind
