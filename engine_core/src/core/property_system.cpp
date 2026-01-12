/**
 * @file property_system.cpp
 * @brief Property Introspection System implementation
 */

#include "NovelMind/core/property_system.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace NovelMind {

// ============================================================================
// TypeInfo
// ============================================================================

TypeInfo::TypeInfo(const std::string& typeName) : m_typeName(typeName) {}

TypeInfo::~TypeInfo() = default;

void TypeInfo::addProperty(std::unique_ptr<IPropertyAccessor> accessor) {
  const auto& name = accessor->getMeta().name;
  m_propertyIndex[name] = m_properties.size();
  m_properties.push_back(std::move(accessor));
}

const std::vector<std::unique_ptr<IPropertyAccessor>>& TypeInfo::getProperties() const {
  return m_properties;
}

const IPropertyAccessor* TypeInfo::findProperty(const std::string& name) const {
  auto it = m_propertyIndex.find(name);
  if (it != m_propertyIndex.end()) {
    return m_properties[it->second].get();
  }
  return nullptr;
}

std::vector<std::pair<std::string, std::vector<const IPropertyAccessor*>>>
TypeInfo::getPropertiesByCategory() const {
  // Collect properties by category
  std::unordered_map<std::string, std::vector<const IPropertyAccessor*>> categoryMap;

  for (const auto& prop : m_properties) {
    const auto& meta = prop->getMeta();
    if (!hasFlag(meta.flags, PropertyFlags::Hidden)) {
      std::string category = meta.category.empty() ? "General" : meta.category;
      categoryMap[category].push_back(prop.get());
    }
  }

  // Sort properties within each category by order
  for (auto& [cat, props] : categoryMap) {
    std::sort(props.begin(), props.end(),
              [](const IPropertyAccessor* a, const IPropertyAccessor* b) {
                return a->getMeta().order < b->getMeta().order;
              });
  }

  // Convert to vector and sort categories
  std::vector<std::pair<std::string, std::vector<const IPropertyAccessor*>>> result;
  result.reserve(categoryMap.size());

  for (auto& [cat, props] : categoryMap) {
    result.emplace_back(cat, std::move(props));
  }

  // Sort categories (General first, then alphabetically)
  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    if (a.first == "General")
      return true;
    if (b.first == "General")
      return false;
    return a.first < b.first;
  });

  return result;
}

// ============================================================================
// PropertyRegistry
// ============================================================================

PropertyRegistry& PropertyRegistry::instance() {
  static PropertyRegistry registry;
  return registry;
}

void PropertyRegistry::registerType(const std::type_index& type, std::unique_ptr<TypeInfo> info) {
  m_types[type] = std::move(info);
}

const TypeInfo* PropertyRegistry::getTypeInfo(const std::type_index& type) const {
  auto it = m_types.find(type);
  if (it != m_types.end()) {
    return it->second.get();
  }
  return nullptr;
}

// ============================================================================
// PropertyUtils
// ============================================================================

namespace PropertyUtils {

std::string toString(const PropertyValue& value) {
  return std::visit(
      [](const auto& v) -> std::string {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, std::nullptr_t>) {
          return "null";
        } else if constexpr (std::is_same_v<T, bool>) {
          return v ? "true" : "false";
        } else if constexpr (std::is_same_v<T, i32>) {
          return std::to_string(v);
        } else if constexpr (std::is_same_v<T, i64>) {
          return std::to_string(v);
        } else if constexpr (std::is_same_v<T, f32>) {
          std::ostringstream oss;
          oss << std::setprecision(6) << v;
          return oss.str();
        } else if constexpr (std::is_same_v<T, f64>) {
          std::ostringstream oss;
          oss << std::setprecision(10) << v;
          return oss.str();
        } else if constexpr (std::is_same_v<T, std::string>) {
          return v;
        } else if constexpr (std::is_same_v<T, Vector2>) {
          std::ostringstream oss;
          oss << std::setprecision(6) << v.x << "," << v.y;
          return oss.str();
        } else if constexpr (std::is_same_v<T, Vector3>) {
          std::ostringstream oss;
          oss << std::setprecision(6) << v.x << "," << v.y << "," << v.z;
          return oss.str();
        } else if constexpr (std::is_same_v<T, Color>) {
          std::ostringstream oss;
          oss << "#" << std::hex << std::setfill('0') << std::setw(8) << v.toHex(true);
          return oss.str();
        } else if constexpr (std::is_same_v<T, AssetRef>) {
          return v.path;
        } else if constexpr (std::is_same_v<T, CurveRef>) {
          return v.curveId;
        } else if constexpr (std::is_same_v<T, EnumValue>) {
          return v.name;
        } else if constexpr (std::is_same_v<T, MultipleValues>) {
          return "<multiple values>";
        } else {
          return "";
        }
      },
      value);
}

PropertyValue fromString(PropertyType type, const std::string& str) {
  switch (type) {
  case PropertyType::Bool:
    return str == "true" || str == "1" || str == "yes";

  case PropertyType::Int:
    try {
      return static_cast<i32>(std::stoi(str));
    } catch (...) {
      return i32{0};
    }

  case PropertyType::Int64:
    try {
      return static_cast<i64>(std::stoll(str));
    } catch (...) {
      return i64{0};
    }

  case PropertyType::Float:
    try {
      return static_cast<f32>(std::stof(str));
    } catch (...) {
      return f32{0.0f};
    }

  case PropertyType::Double:
    try {
      return std::stod(str);
    } catch (...) {
      return f64{0.0};
    }

  case PropertyType::String:
    return str;

  case PropertyType::Vector2: {
    Vector2 v;
    std::istringstream iss(str);
    char comma;
    if (iss >> v.x >> comma >> v.y) {
      return v;
    }
    return Vector2{};
  }

  case PropertyType::Vector3: {
    Vector3 v;
    std::istringstream iss(str);
    char comma1, comma2;
    if (iss >> v.x >> comma1 >> v.y >> comma2 >> v.z) {
      return v;
    }
    return Vector3{};
  }

  case PropertyType::Color: {
    if (str.empty())
      return Color{};

    std::string hex = str;
    if (hex[0] == '#') {
      hex = hex.substr(1);
    }

    try {
      u32 value = static_cast<u32>(std::stoul(hex, nullptr, 16));
      bool hasAlpha = hex.length() > 6;
      return Color::fromHex(value, hasAlpha);
    } catch (...) {
      return Color{};
    }
  }

  case PropertyType::AssetRef:
    return AssetRef{"", str};

  case PropertyType::CurveRef:
    return CurveRef{str};

  case PropertyType::Enum: {
    EnumValue ev;
    try {
      ev.value = std::stoi(str);
    } catch (...) {
      ev.name = str;
    }
    return ev;
  }

  default:
    return nullptr;
  }
}

const char* getTypeName(PropertyType type) {
  switch (type) {
  case PropertyType::None:
    return "None";
  case PropertyType::Bool:
    return "Bool";
  case PropertyType::Int:
    return "Int";
  case PropertyType::Int64:
    return "Int64";
  case PropertyType::Float:
    return "Float";
  case PropertyType::Double:
    return "Double";
  case PropertyType::String:
    return "String";
  case PropertyType::Vector2:
    return "Vector2";
  case PropertyType::Vector3:
    return "Vector3";
  case PropertyType::Color:
    return "Color";
  case PropertyType::Enum:
    return "Enum";
  case PropertyType::AssetRef:
    return "AssetRef";
  case PropertyType::CurveRef:
    return "CurveRef";
  default:
    return "Unknown";
  }
}

bool validate(const PropertyValue& value, const PropertyMeta& meta, std::string* error) {
  // Check required flag
  if (hasFlag(meta.flags, PropertyFlags::Required)) {
    bool empty = std::visit(
        [](const auto& v) -> bool {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, std::nullptr_t>)
            return true;
          else if constexpr (std::is_same_v<T, std::string>)
            return v.empty();
          else if constexpr (std::is_same_v<T, AssetRef>)
            return v.isEmpty();
          else if constexpr (std::is_same_v<T, CurveRef>)
            return v.isEmpty();
          else
            return false;
        },
        value);

    if (empty) {
      if (error) {
        *error = "Property '" + meta.displayName + "' is required";
      }
      return false;
    }
  }

  // Validate range for numeric types
  if (meta.range.hasMin || meta.range.hasMax) {
    f64 numValue = 0.0;
    bool isNumeric = std::visit(
        [&numValue](const auto& v) -> bool {
          using T = std::decay_t<decltype(v)>;
          if constexpr (std::is_same_v<T, i32>) {
            numValue = static_cast<f64>(v);
            return true;
          } else if constexpr (std::is_same_v<T, i64>) {
            numValue = static_cast<f64>(v);
            return true;
          } else if constexpr (std::is_same_v<T, f32>) {
            numValue = static_cast<f64>(v);
            return true;
          } else if constexpr (std::is_same_v<T, f64>) {
            numValue = v;
            return true;
          } else {
            return false;
          }
        },
        value);

    if (isNumeric) {
      if (meta.range.hasMin && numValue < meta.range.min) {
        if (error) {
          *error = "Value must be at least " + std::to_string(meta.range.min);
        }
        return false;
      }

      if (meta.range.hasMax && numValue > meta.range.max) {
        if (error) {
          *error = "Value must be at most " + std::to_string(meta.range.max);
        }
        return false;
      }
    }
  }

  // Validate enum values
  if (meta.type == PropertyType::Enum && !meta.enumOptions.empty()) {
    if (auto* ev = std::get_if<EnumValue>(&value)) {
      bool found = false;
      for (const auto& [val, name] : meta.enumOptions) {
        if (ev->value == val) {
          found = true;
          break;
        }
      }

      if (!found) {
        if (error) {
          *error = "Invalid enum value";
        }
        return false;
      }
    }
  }

  return true;
}

PropertyValue clampToRange(const PropertyValue& value, const RangeConstraint& range) {
  return std::visit(
      [&range](const auto& v) -> PropertyValue {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, i32>) {
          f64 clamped = static_cast<f64>(v);
          if (range.hasMin)
            clamped = std::max(clamped, range.min);
          if (range.hasMax)
            clamped = std::min(clamped, range.max);
          if (range.step > 0) {
            clamped = std::round(clamped / range.step) * range.step;
          }
          return static_cast<i32>(clamped);
        } else if constexpr (std::is_same_v<T, i64>) {
          f64 clamped = static_cast<f64>(v);
          if (range.hasMin)
            clamped = std::max(clamped, range.min);
          if (range.hasMax)
            clamped = std::min(clamped, range.max);
          if (range.step > 0) {
            clamped = std::round(clamped / range.step) * range.step;
          }
          return static_cast<i64>(clamped);
        } else if constexpr (std::is_same_v<T, f32>) {
          f64 clamped = static_cast<f64>(v);
          if (range.hasMin)
            clamped = std::max(clamped, range.min);
          if (range.hasMax)
            clamped = std::min(clamped, range.max);
          if (range.step > 0) {
            clamped = std::round(clamped / range.step) * range.step;
          }
          return static_cast<f32>(clamped);
        } else if constexpr (std::is_same_v<T, f64>) {
          f64 clamped = v;
          if (range.hasMin)
            clamped = std::max(clamped, range.min);
          if (range.hasMax)
            clamped = std::min(clamped, range.max);
          if (range.step > 0) {
            clamped = std::round(clamped / range.step) * range.step;
          }
          return clamped;
        } else {
          return v;
        }
      },
      value);
}

} // namespace PropertyUtils

} // namespace NovelMind
