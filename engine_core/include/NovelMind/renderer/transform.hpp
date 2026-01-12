#pragma once

#include "NovelMind/core/types.hpp"

namespace NovelMind::renderer {

struct Vec2 {
  f32 x;
  f32 y;

  constexpr Vec2() : x(0.0f), y(0.0f) {}
  constexpr Vec2(f32 x_, f32 y_) : x(x_), y(y_) {}

  constexpr Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }

  constexpr Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }

  constexpr Vec2 operator*(f32 scalar) const { return Vec2(x * scalar, y * scalar); }

  constexpr Vec2 operator/(f32 scalar) const { return Vec2(x / scalar, y / scalar); }

  static const Vec2 Zero;
  static const Vec2 One;
  static const Vec2 UnitX;
  static const Vec2 UnitY;
};

inline constexpr Vec2 Vec2::Zero{0.0f, 0.0f};
inline constexpr Vec2 Vec2::One{1.0f, 1.0f};
inline constexpr Vec2 Vec2::UnitX{1.0f, 0.0f};
inline constexpr Vec2 Vec2::UnitY{0.0f, 1.0f};

struct Rect {
  f32 x;
  f32 y;
  f32 width;
  f32 height;

  constexpr Rect() : x(0.0f), y(0.0f), width(0.0f), height(0.0f) {}

  constexpr Rect(f32 x_, f32 y_, f32 w, f32 h) : x(x_), y(y_), width(w), height(h) {}

  [[nodiscard]] constexpr bool contains(f32 px, f32 py) const {
    return px >= x && px < x + width && py >= y && py < y + height;
  }

  [[nodiscard]] constexpr bool contains(const Vec2& point) const {
    return contains(point.x, point.y);
  }
};

struct Transform2D {
  f32 x = 0.0f;
  f32 y = 0.0f;
  f32 scaleX = 1.0f;
  f32 scaleY = 1.0f;
  f32 rotation = 0.0f;
  f32 anchorX = 0.0f;
  f32 anchorY = 0.0f;

  [[nodiscard]] Vec2 getPosition() const { return Vec2(x, y); }

  void setPosition(const Vec2& pos) {
    x = pos.x;
    y = pos.y;
  }

  void setPosition(f32 px, f32 py) {
    x = px;
    y = py;
  }

  [[nodiscard]] Vec2 getScale() const { return Vec2(scaleX, scaleY); }

  void setScale(f32 sx, f32 sy) {
    scaleX = sx;
    scaleY = sy;
  }

  void setScale(f32 uniform) {
    scaleX = uniform;
    scaleY = uniform;
  }

  [[nodiscard]] Vec2 getAnchor() const { return Vec2(anchorX, anchorY); }

  void setAnchor(f32 ax, f32 ay) {
    anchorX = ax;
    anchorY = ay;
  }
};

} // namespace NovelMind::renderer
