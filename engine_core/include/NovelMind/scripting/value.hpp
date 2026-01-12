#pragma once

#include "NovelMind/core/types.hpp"
#include <string>
#include <variant>

namespace NovelMind::scripting {

using Value = std::variant<std::monostate, i32, f32, bool, std::string>;

enum class ValueType { Null, Int, Float, Bool, String };

inline ValueType getValueType(const Value& val) {
  if (std::holds_alternative<std::monostate>(val))
    return ValueType::Null;
  if (std::holds_alternative<i32>(val))
    return ValueType::Int;
  if (std::holds_alternative<f32>(val))
    return ValueType::Float;
  if (std::holds_alternative<bool>(val))
    return ValueType::Bool;
  if (std::holds_alternative<std::string>(val))
    return ValueType::String;
  return ValueType::Null;
}

inline bool isNull(const Value& val) {
  return std::holds_alternative<std::monostate>(val);
}

inline i32 asInt(const Value& val) {
  if (auto* p = std::get_if<i32>(&val))
    return *p;
  if (auto* p = std::get_if<f32>(&val))
    return static_cast<i32>(*p);
  if (auto* p = std::get_if<bool>(&val))
    return *p ? 1 : 0;
  return 0;
}

inline f32 asFloat(const Value& val) {
  if (auto* p = std::get_if<f32>(&val))
    return *p;
  if (auto* p = std::get_if<i32>(&val))
    return static_cast<f32>(*p);
  if (auto* p = std::get_if<bool>(&val))
    return *p ? 1.0f : 0.0f;
  return 0.0f;
}

inline bool asBool(const Value& val) {
  if (auto* p = std::get_if<bool>(&val))
    return *p;
  if (auto* p = std::get_if<i32>(&val))
    return *p != 0;
  if (auto* p = std::get_if<f32>(&val))
    return *p != 0.0f;
  if (auto* p = std::get_if<std::string>(&val))
    return !p->empty();
  return false;
}

inline std::string asString(const Value& val) {
  if (auto* p = std::get_if<std::string>(&val))
    return *p;
  if (auto* p = std::get_if<i32>(&val))
    return std::to_string(*p);
  if (auto* p = std::get_if<f32>(&val))
    return std::to_string(*p);
  if (auto* p = std::get_if<bool>(&val))
    return *p ? "true" : "false";
  return "null";
}

} // namespace NovelMind::scripting
