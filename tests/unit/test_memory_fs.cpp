#include <catch2/catch_test_macros.hpp>
#include "NovelMind/vfs/memory_fs.hpp"

using namespace NovelMind;
using namespace NovelMind::vfs;

TEST_CASE("MemoryFS add and read resource", "[vfs]") {
  MemoryFileSystem fs;

  std::vector<u8> data = {1, 2, 3, 4, 5};
  fs.addResource("test_resource", data, ResourceType::Data);

  auto result = fs.readFile("test_resource");
  REQUIRE(result.isOk());
  REQUIRE(result.value() == data);
}

TEST_CASE("MemoryFS exists returns true for existing resource", "[vfs]") {
  MemoryFileSystem fs;

  fs.addResource("test", {1, 2, 3}, ResourceType::Data);

  REQUIRE(fs.exists("test"));
}

TEST_CASE("MemoryFS exists returns false for non-existing resource", "[vfs]") {
  MemoryFileSystem fs;

  REQUIRE_FALSE(fs.exists("nonexistent"));
}

TEST_CASE("MemoryFS readFile returns error for non-existing resource", "[vfs]") {
  MemoryFileSystem fs;

  auto result = fs.readFile("nonexistent");
  REQUIRE(result.isError());
}

TEST_CASE("MemoryFS getInfo returns resource info", "[vfs]") {
  MemoryFileSystem fs;

  std::vector<u8> data = {1, 2, 3, 4, 5};
  fs.addResource("texture", data, ResourceType::Texture);

  auto info = fs.getInfo("texture");
  REQUIRE(info.has_value());
  REQUIRE(info->id == "texture");
  REQUIRE(info->type == ResourceType::Texture);
  REQUIRE(info->size == 5);
}

TEST_CASE("MemoryFS listResources returns all resources", "[vfs]") {
  MemoryFileSystem fs;

  fs.addResource("res1", {1}, ResourceType::Data);
  fs.addResource("res2", {2}, ResourceType::Texture);
  fs.addResource("res3", {3}, ResourceType::Audio);

  auto all = fs.listResources();
  REQUIRE(all.size() == 3);
}

TEST_CASE("MemoryFS listResources filters by type", "[vfs]") {
  MemoryFileSystem fs;

  fs.addResource("data1", {1}, ResourceType::Data);
  fs.addResource("tex1", {2}, ResourceType::Texture);
  fs.addResource("tex2", {3}, ResourceType::Texture);

  auto textures = fs.listResources(ResourceType::Texture);
  REQUIRE(textures.size() == 2);
}

TEST_CASE("MemoryFS removeResource removes resource", "[vfs]") {
  MemoryFileSystem fs;

  fs.addResource("test", {1, 2, 3}, ResourceType::Data);
  REQUIRE(fs.exists("test"));

  fs.removeResource("test");
  REQUIRE_FALSE(fs.exists("test"));
}

TEST_CASE("MemoryFS clear removes all resources", "[vfs]") {
  MemoryFileSystem fs;

  fs.addResource("res1", {1}, ResourceType::Data);
  fs.addResource("res2", {2}, ResourceType::Data);

  fs.clear();

  REQUIRE(fs.listResources().empty());
}

TEST_CASE("MemoryFS unmountAll clears all resources", "[vfs]") {
  MemoryFileSystem fs;

  fs.addResource("res", {1}, ResourceType::Data);
  fs.unmountAll();

  REQUIRE_FALSE(fs.exists("res"));
}
