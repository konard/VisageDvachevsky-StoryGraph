#include <catch2/catch_test_macros.hpp>
#include "NovelMind/core/result.hpp"

using namespace NovelMind;

TEST_CASE("Result<T> ok value", "[result]") {
  auto result = Result<int>::ok(42);

  REQUIRE(result.isOk());
  REQUIRE_FALSE(result.isError());
  REQUIRE(result.value() == 42);
}

TEST_CASE("Result<T> error value", "[result]") {
  auto result = Result<int>::error("Something went wrong");

  REQUIRE_FALSE(result.isOk());
  REQUIRE(result.isError());
  REQUIRE(result.error() == "Something went wrong");
}

TEST_CASE("Result<T> valueOr returns value when ok", "[result]") {
  auto result = Result<int>::ok(42);

  REQUIRE(result.valueOr(0) == 42);
}

TEST_CASE("Result<T> valueOr returns default when error", "[result]") {
  auto result = Result<int>::error("error");

  REQUIRE(result.valueOr(100) == 100);
}

TEST_CASE("Result<void> ok", "[result]") {
  auto result = Result<void>::ok();

  REQUIRE(result.isOk());
  REQUIRE_FALSE(result.isError());
}

TEST_CASE("Result<void> error", "[result]") {
  auto result = Result<void>::error("Something went wrong");

  REQUIRE_FALSE(result.isOk());
  REQUIRE(result.isError());
  REQUIRE(result.error() == "Something went wrong");
}

TEST_CASE("Result<T> with string value", "[result]") {
  auto result = Result<std::string>::ok("hello world");

  REQUIRE(result.isOk());
  REQUIRE(result.value() == "hello world");
}

TEST_CASE("Result<T> move semantics", "[result]") {
  auto result = Result<std::string>::ok("test");
  std::string moved = std::move(result).value();

  REQUIRE(moved == "test");
}
