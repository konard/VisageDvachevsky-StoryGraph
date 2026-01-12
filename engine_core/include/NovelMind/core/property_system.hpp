#pragma once

/**
 * @file property_system.hpp
 * @brief Property Introspection System for NovelMind Editor
 *
 * This system provides runtime type information and metadata for properties,
 * enabling the Inspector panel to:
 * - Display appropriate UI controls for each property type
 * - Validate property values
 * - Support undo/redo operations
 * - Group properties by category
 *
 * Supported property types:
 * - Int (with min/max/step)
 * - Float (with min/max/step)
 * - Bool
 * - String
 * - Enum
 * - Color (RGBA)
 * - Vector2/Vector3
 * - AssetRef (reference to assets)
 * - CurveRef (reference to animation curves)
 *
 * Supported attributes:
 * - [Range(min, max)]
 * - [Step(value)]
 * - [Foldout]
 * - [Hidden]
 * - [Category]
 * - [ReadOnly]
 * - [Tooltip]
 */

#include "NovelMind/core/types.hpp"
#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <variant>
#include <vector>

namespace NovelMind {

// Forward declarations
class PropertyRegistry;

/**
 * @brief 2D Vector for property values
 */
struct Vector2 {
  f32 x = 0.0f;
  f32 y = 0.0f;

  Vector2() = default;
  Vector2(f32 x_, f32 y_) : x(x_), y(y_) {}

  bool operator==(const Vector2& other) const { return x == other.x && y == other.y; }

  bool operator!=(const Vector2& other) const { return !(*this == other); }
};

/**
 * @brief 3D Vector for property values
 */
struct Vector3 {
  f32 x = 0.0f;
  f32 y = 0.0f;
  f32 z = 0.0f;

  Vector3() = default;
  Vector3(f32 x_, f32 y_, f32 z_) : x(x_), y(y_), z(z_) {}

  bool operator==(const Vector3& other) const {
    return x == other.x && y == other.y && z == other.z;
  }

  bool operator!=(const Vector3& other) const { return !(*this == other); }
};

/**
 * @brief Color value (RGBA)
 */
struct Color {
  f32 r = 1.0f;
  f32 g = 1.0f;
  f32 b = 1.0f;
  f32 a = 1.0f;

  Color() = default;
  Color(f32 r_, f32 g_, f32 b_, f32 a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}

  /**
   * @brief Create color from hex value (0xRRGGBB or 0xRRGGBBAA)
   */
  static Color fromHex(u32 hex, bool hasAlpha = false) {
    if (hasAlpha) {
      return Color(static_cast<f32>((hex >> 24) & 0xFF) / 255.0f,
                   static_cast<f32>((hex >> 16) & 0xFF) / 255.0f,
                   static_cast<f32>((hex >> 8) & 0xFF) / 255.0f,
                   static_cast<f32>(hex & 0xFF) / 255.0f);
    }
    return Color(static_cast<f32>((hex >> 16) & 0xFF) / 255.0f,
                 static_cast<f32>((hex >> 8) & 0xFF) / 255.0f,
                 static_cast<f32>(hex & 0xFF) / 255.0f, 1.0f);
  }

  /**
   * @brief Convert to hex value
   */
  [[nodiscard]] u32 toHex(bool includeAlpha = false) const {
    u8 ri = static_cast<u8>(r * 255.0f);
    u8 gi = static_cast<u8>(g * 255.0f);
    u8 bi = static_cast<u8>(b * 255.0f);
    u8 ai = static_cast<u8>(a * 255.0f);

    if (includeAlpha) {
      return (static_cast<u32>(ri) << 24) | (static_cast<u32>(gi) << 16) |
             (static_cast<u32>(bi) << 8) | static_cast<u32>(ai);
    }
    return (static_cast<u32>(ri) << 16) | (static_cast<u32>(gi) << 8) | static_cast<u32>(bi);
  }

  bool operator==(const Color& other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }

  bool operator!=(const Color& other) const { return !(*this == other); }
};

/**
 * @brief Reference to an asset
 */
struct AssetRef {
  std::string assetType; // "texture", "audio", "font", etc.
  std::string path;      // Path to the asset
  std::string uuid;      // Optional UUID for asset tracking

  AssetRef() = default;
  AssetRef(const std::string& type, const std::string& assetPath)
      : assetType(type), path(assetPath) {}

  [[nodiscard]] bool isEmpty() const { return path.empty(); }

  bool operator==(const AssetRef& other) const {
    return assetType == other.assetType && path == other.path;
  }

  bool operator!=(const AssetRef& other) const { return !(*this == other); }
};

/**
 * @brief Reference to an animation curve
 */
struct CurveRef {
  std::string curveId;
  std::string curveName;

  CurveRef() = default;
  explicit CurveRef(const std::string& id) : curveId(id) {}

  [[nodiscard]] bool isEmpty() const { return curveId.empty(); }

  bool operator==(const CurveRef& other) const { return curveId == other.curveId; }

  bool operator!=(const CurveRef& other) const { return !(*this == other); }
};

/**
 * @brief Enum value with options
 */
struct EnumValue {
  i32 value = 0;
  std::string name;
  std::vector<std::pair<i32, std::string>> options;

  EnumValue() = default;
  EnumValue(i32 val, const std::string& n) : value(val), name(n) {}

  bool operator==(const EnumValue& other) const { return value == other.value; }

  bool operator!=(const EnumValue& other) const { return !(*this == other); }
};

/**
 * @brief Marker for "multiple values" in multi-object editing
 * When inspecting multiple objects, if a property has different values across
 * the objects, this marker is used to indicate "multiple values" in the UI.
 */
struct MultipleValues {
  bool operator==(const MultipleValues&) const { return true; }
  bool operator!=(const MultipleValues&) const { return false; }
};

/**
 * @brief Property value variant type
 */
using PropertyValue = std::variant<std::nullptr_t, bool, i32, i64, f32, f64, std::string, Vector2,
                                   Vector3, Color, AssetRef, CurveRef, EnumValue, MultipleValues>;

/**
 * @brief Property type enumeration
 */
enum class PropertyType : u8 {
  None = 0,
  Bool,
  Int,
  Int64,
  Float,
  Double,
  String,
  Vector2,
  Vector3,
  Color,
  Enum,
  AssetRef,
  CurveRef
};

/**
 * @brief Property attribute flags
 */
enum class PropertyFlags : u32 {
  None = 0,
  ReadOnly = 1 << 0,
  Hidden = 1 << 1,
  Foldout = 1 << 2,     // Collapsible group header
  FoldoutEnd = 1 << 3,  // End of foldout group
  Slider = 1 << 4,      // Show as slider instead of input
  ColorPicker = 1 << 5, // Show color picker for Color type
  FilePicker = 1 << 6,  // Show file picker for AssetRef
  MultiLine = 1 << 7,   // Multi-line text editor for String
  Password = 1 << 8,    // Hide text (for sensitive strings)
  Angle = 1 << 9,       // Interpret float as angle (show dial)
  Percentage = 1 << 10, // Show as percentage (0-100)
  Normalized = 1 << 11, // Value is normalized (0-1)
  NoUndo = 1 << 12,     // Don't record undo for this property
  Transient = 1 << 13,  // Don't serialize this property
  Required = 1 << 14    // Property is required (highlight if empty)
};

/**
 * @brief Bitwise OR operator for PropertyFlags
 */
inline PropertyFlags operator|(PropertyFlags a, PropertyFlags b) {
  return static_cast<PropertyFlags>(static_cast<u32>(a) | static_cast<u32>(b));
}

/**
 * @brief Bitwise AND operator for PropertyFlags
 */
inline PropertyFlags operator&(PropertyFlags a, PropertyFlags b) {
  return static_cast<PropertyFlags>(static_cast<u32>(a) & static_cast<u32>(b));
}

/**
 * @brief Check if flag is set
 */
inline bool hasFlag(PropertyFlags flags, PropertyFlags flag) {
  return (static_cast<u32>(flags) & static_cast<u32>(flag)) != 0;
}

/**
 * @brief Range constraint for numeric properties
 */
struct RangeConstraint {
  f64 min = 0.0;
  f64 max = 1.0;
  f64 step = 0.0; // 0 = no step constraint
  bool hasMin = false;
  bool hasMax = false;

  RangeConstraint() = default;
  RangeConstraint(f64 minVal, f64 maxVal, f64 stepVal = 0.0)
      : min(minVal), max(maxVal), step(stepVal), hasMin(true), hasMax(true) {}

  static RangeConstraint withMin(f64 minVal) {
    RangeConstraint r;
    r.min = minVal;
    r.hasMin = true;
    return r;
  }

  static RangeConstraint withMax(f64 maxVal) {
    RangeConstraint r;
    r.max = maxVal;
    r.hasMax = true;
    return r;
  }
};

/**
 * @brief Property metadata describing a single property
 */
struct PropertyMeta {
  std::string name;        // Internal name
  std::string displayName; // Display name for UI
  std::string category;    // Category for grouping
  std::string tooltip;     // Help text
  PropertyType type = PropertyType::None;
  PropertyFlags flags = PropertyFlags::None;
  PropertyValue defaultValue;
  RangeConstraint range;
  std::vector<std::pair<i32, std::string>> enumOptions; // For Enum type
  std::string assetFilter; // File filter for AssetRef (e.g., "*.png;*.jpg")
  i32 order = 0;           // Display order within category

  PropertyMeta() = default;
  PropertyMeta(const std::string& n, const std::string& display, PropertyType t)
      : name(n), displayName(display), type(t) {}
};

/**
 * @brief Property accessor interface
 */
class IPropertyAccessor {
public:
  virtual ~IPropertyAccessor() = default;

  /**
   * @brief Get property value from object
   */
  [[nodiscard]] virtual PropertyValue getValue(const void* object) const = 0;

  /**
   * @brief Set property value on object
   */
  virtual void setValue(void* object, const PropertyValue& value) const = 0;

  /**
   * @brief Get the property metadata
   */
  [[nodiscard]] virtual const PropertyMeta& getMeta() const = 0;
};

/**
 * @brief Type-safe property accessor implementation
 */
template <typename T, typename PropType> class PropertyAccessor : public IPropertyAccessor {
public:
  using Getter = std::function<PropType(const T&)>;
  using Setter = std::function<void(T&, const PropType&)>;

  PropertyAccessor(const PropertyMeta& meta, Getter getter, Setter setter)
      : m_meta(meta), m_getter(std::move(getter)), m_setter(std::move(setter)) {}

  [[nodiscard]] PropertyValue getValue(const void* object) const override {
    const auto* obj = static_cast<const T*>(object);
    return PropertyValue(m_getter(*obj));
  }

  void setValue(void* object, const PropertyValue& value) const override {
    auto* obj = static_cast<T*>(object);
    if (auto* val = std::get_if<PropType>(&value)) {
      m_setter(*obj, *val);
    }
  }

  [[nodiscard]] const PropertyMeta& getMeta() const override { return m_meta; }

private:
  PropertyMeta m_meta;
  Getter m_getter;
  Setter m_setter;
};

/**
 * @brief Type information for a class with inspectable properties
 */
class TypeInfo {
public:
  explicit TypeInfo(const std::string& typeName);
  ~TypeInfo();

  /**
   * @brief Register a property accessor
   */
  void addProperty(std::unique_ptr<IPropertyAccessor> accessor);

  /**
   * @brief Get all property accessors
   */
  [[nodiscard]] const std::vector<std::unique_ptr<IPropertyAccessor>>& getProperties() const;

  /**
   * @brief Find property by name
   */
  [[nodiscard]] const IPropertyAccessor* findProperty(const std::string& name) const;

  /**
   * @brief Get type name
   */
  [[nodiscard]] const std::string& getTypeName() const { return m_typeName; }

  /**
   * @brief Get properties grouped by category
   */
  [[nodiscard]] std::vector<std::pair<std::string, std::vector<const IPropertyAccessor*>>>
  getPropertiesByCategory() const;

private:
  std::string m_typeName;
  std::vector<std::unique_ptr<IPropertyAccessor>> m_properties;
  std::unordered_map<std::string, size_t> m_propertyIndex;
};

/**
 * @brief Global property registry
 */
class PropertyRegistry {
public:
  /**
   * @brief Get singleton instance
   */
  static PropertyRegistry& instance();

  /**
   * @brief Register type information
   */
  void registerType(const std::type_index& type, std::unique_ptr<TypeInfo> info);

  /**
   * @brief Get type information
   */
  [[nodiscard]] const TypeInfo* getTypeInfo(const std::type_index& type) const;

  /**
   * @brief Template version for convenience
   */
  template <typename T> [[nodiscard]] const TypeInfo* getTypeInfo() const {
    return getTypeInfo(std::type_index(typeid(T)));
  }

  /**
   * @brief Template version for registration
   */
  template <typename T> void registerType(std::unique_ptr<TypeInfo> info) {
    registerType(std::type_index(typeid(T)), std::move(info));
  }

private:
  PropertyRegistry() = default;
  std::unordered_map<std::type_index, std::unique_ptr<TypeInfo>> m_types;
};

/**
 * @brief Builder for creating TypeInfo with fluent API
 */
template <typename T> class TypeInfoBuilder {
public:
  explicit TypeInfoBuilder(const std::string& typeName)
      : m_info(std::make_unique<TypeInfo>(typeName)) {}

  /**
   * @brief Add a property with getter/setter
   */
  template <typename PropType>
  TypeInfoBuilder& property(const std::string& name, const std::string& displayName,
                            std::function<PropType(const T&)> getter,
                            std::function<void(T&, const PropType&)> setter) {
    PropertyMeta meta(name, displayName, deducePropertyType<PropType>());
    meta.order = m_orderCounter++;
    m_info->addProperty(std::make_unique<PropertyAccessor<T, PropType>>(meta, std::move(getter),
                                                                        std::move(setter)));
    return *this;
  }

  /**
   * @brief Add a property with metadata
   */
  template <typename PropType>
  TypeInfoBuilder& property(PropertyMeta meta, std::function<PropType(const T&)> getter,
                            std::function<void(T&, const PropType&)> setter) {
    meta.type = deducePropertyType<PropType>();
    if (meta.order == 0) {
      meta.order = m_orderCounter++;
    }
    m_info->addProperty(std::make_unique<PropertyAccessor<T, PropType>>(meta, std::move(getter),
                                                                        std::move(setter)));
    return *this;
  }

  /**
   * @brief Build and register the type info
   */
  void build() { PropertyRegistry::instance().registerType<T>(std::move(m_info)); }

  /**
   * @brief Get the built TypeInfo without registering
   */
  std::unique_ptr<TypeInfo> get() { return std::move(m_info); }

private:
  template <typename PropType> static PropertyType deducePropertyType() {
    if constexpr (std::is_same_v<PropType, bool>)
      return PropertyType::Bool;
    else if constexpr (std::is_same_v<PropType, i32>)
      return PropertyType::Int;
    else if constexpr (std::is_same_v<PropType, i64>)
      return PropertyType::Int64;
    else if constexpr (std::is_same_v<PropType, f32>)
      return PropertyType::Float;
    else if constexpr (std::is_same_v<PropType, f64>)
      return PropertyType::Double;
    else if constexpr (std::is_same_v<PropType, std::string>)
      return PropertyType::String;
    else if constexpr (std::is_same_v<PropType, Vector2>)
      return PropertyType::Vector2;
    else if constexpr (std::is_same_v<PropType, Vector3>)
      return PropertyType::Vector3;
    else if constexpr (std::is_same_v<PropType, Color>)
      return PropertyType::Color;
    else if constexpr (std::is_same_v<PropType, EnumValue>)
      return PropertyType::Enum;
    else if constexpr (std::is_same_v<PropType, AssetRef>)
      return PropertyType::AssetRef;
    else if constexpr (std::is_same_v<PropType, CurveRef>)
      return PropertyType::CurveRef;
    else
      return PropertyType::None;
  }

  std::unique_ptr<TypeInfo> m_info;
  i32 m_orderCounter = 0;
};

/**
 * @brief Helper macro to start type registration
 */
#define NM_BEGIN_TYPE(TypeName) NovelMind::TypeInfoBuilder<TypeName>(#TypeName)

/**
 * @brief Helper macro for simple property registration
 */
#define NM_PROPERTY(name, getter, setter) .property(#name, #name, getter, setter)

/**
 * @brief Utility functions for property value conversion
 */
namespace PropertyUtils {
/**
 * @brief Convert property value to string
 */
std::string toString(const PropertyValue& value);

/**
 * @brief Parse property value from string
 */
PropertyValue fromString(PropertyType type, const std::string& str);

/**
 * @brief Get property type name
 */
const char* getTypeName(PropertyType type);

/**
 * @brief Validate property value against constraints
 */
bool validate(const PropertyValue& value, const PropertyMeta& meta, std::string* error = nullptr);

/**
 * @brief Clamp value to range constraint
 */
PropertyValue clampToRange(const PropertyValue& value, const RangeConstraint& range);

} // namespace PropertyUtils

} // namespace NovelMind
