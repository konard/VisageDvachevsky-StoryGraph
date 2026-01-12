#include <catch2/catch_test_macros.hpp>
#include "NovelMind/scripting/value.hpp"

using namespace NovelMind::scripting;

TEST_CASE("Value type detection", "[value]") {
  Value null_val = std::monostate{};
  Value int_val = NovelMind::i32{42};
  Value float_val = NovelMind::f32{3.14f};
  Value bool_val = true;
  Value str_val = std::string{"hello"};

  REQUIRE(getValueType(null_val) == ValueType::Null);
  REQUIRE(getValueType(int_val) == ValueType::Int);
  REQUIRE(getValueType(float_val) == ValueType::Float);
  REQUIRE(getValueType(bool_val) == ValueType::Bool);
  REQUIRE(getValueType(str_val) == ValueType::String);
}

TEST_CASE("isNull function", "[value]") {
  Value null_val = std::monostate{};
  Value int_val = NovelMind::i32{42};

  REQUIRE(isNull(null_val));
  REQUIRE_FALSE(isNull(int_val));
}

TEST_CASE("asInt conversions", "[value]") {
  REQUIRE(asInt(NovelMind::i32{42}) == 42);
  REQUIRE(asInt(NovelMind::f32{3.7f}) == 3);
  REQUIRE(asInt(true) == 1);
  REQUIRE(asInt(false) == 0);
  REQUIRE(asInt(std::monostate{}) == 0);
}

TEST_CASE("asFloat conversions", "[value]") {
  REQUIRE(asFloat(NovelMind::f32{3.14f}) == 3.14f);
  REQUIRE(asFloat(NovelMind::i32{42}) == 42.0f);
  REQUIRE(asFloat(true) == 1.0f);
  REQUIRE(asFloat(false) == 0.0f);
}

TEST_CASE("asBool conversions", "[value]") {
  REQUIRE(asBool(true) == true);
  REQUIRE(asBool(false) == false);
  REQUIRE(asBool(NovelMind::i32{1}) == true);
  REQUIRE(asBool(NovelMind::i32{0}) == false);
  REQUIRE(asBool(NovelMind::f32{1.0f}) == true);
  REQUIRE(asBool(NovelMind::f32{0.0f}) == false);
  REQUIRE(asBool(std::string{"hello"}) == true);
  REQUIRE(asBool(std::string{""}) == false);
  REQUIRE(asBool(std::monostate{}) == false);
}

TEST_CASE("asString conversions", "[value]") {
  REQUIRE(asString(std::string{"hello"}) == "hello");
  REQUIRE(asString(NovelMind::i32{42}) == "42");
  REQUIRE(asString(true) == "true");
  REQUIRE(asString(false) == "false");
  REQUIRE(asString(std::monostate{}) == "null");
}
