#include <catch2/catch_test_macros.hpp>
#include "NovelMind/vfs/resource_id.hpp"

#include <unordered_set>

using namespace NovelMind::VFS;

TEST_CASE("ResourceId construction", "[resource][id]")
{
    SECTION("Default construction creates empty ID")
    {
        ResourceId id;
        REQUIRE(id.isEmpty());
        REQUIRE_FALSE(id.isValid());
        REQUIRE(id.id().empty());
        REQUIRE(id.type() == ResourceType::Unknown);
    }

    SECTION("String construction creates valid ID")
    {
        ResourceId id("test_resource");
        REQUIRE_FALSE(id.isEmpty());
        REQUIRE(id.isValid());
        REQUIRE(id.id() == "test_resource");
    }

    SECTION("String with type construction")
    {
        ResourceId id("my_texture", ResourceType::Texture);
        REQUIRE(id.id() == "my_texture");
        REQUIRE(id.type() == ResourceType::Texture);
    }

    SECTION("Empty string creates invalid ID")
    {
        ResourceId id("");
        REQUIRE(id.isEmpty());
        REQUIRE_FALSE(id.isValid());
    }
}

TEST_CASE("ResourceId type detection from extension", "[resource][id][type]")
{
    SECTION("Texture extensions are detected")
    {
        REQUIRE(ResourceId::typeFromExtension("image.png") == ResourceType::Texture);
        REQUIRE(ResourceId::typeFromExtension("photo.jpg") == ResourceType::Texture);
        REQUIRE(ResourceId::typeFromExtension("pic.jpeg") == ResourceType::Texture);
        REQUIRE(ResourceId::typeFromExtension("bitmap.bmp") == ResourceType::Texture);
        REQUIRE(ResourceId::typeFromExtension("texture.tga") == ResourceType::Texture);
    }

    SECTION("Audio extensions are detected")
    {
        REQUIRE(ResourceId::typeFromExtension("sound.wav") == ResourceType::Audio);
        REQUIRE(ResourceId::typeFromExtension("effect.ogg") == ResourceType::Audio);
        REQUIRE(ResourceId::typeFromExtension("voice.mp3") == ResourceType::Audio);
        REQUIRE(ResourceId::typeFromExtension("audio.flac") == ResourceType::Audio);
    }

    SECTION("Font extensions are detected")
    {
        REQUIRE(ResourceId::typeFromExtension("font.ttf") == ResourceType::Font);
        REQUIRE(ResourceId::typeFromExtension("typeface.otf") == ResourceType::Font);
    }

    SECTION("Script extensions are detected")
    {
        REQUIRE(ResourceId::typeFromExtension("story.nms") == ResourceType::Script);
        REQUIRE(ResourceId::typeFromExtension("code.nmscript") == ResourceType::Script);
    }

    SECTION("Scene extensions are detected")
    {
        REQUIRE(ResourceId::typeFromExtension("level.nmscene") == ResourceType::Scene);
        REQUIRE(ResourceId::typeFromExtension("area.scene") == ResourceType::Scene);
    }

    SECTION("Localization extensions are detected")
    {
        REQUIRE(ResourceId::typeFromExtension("strings.json") == ResourceType::Localization);
        REQUIRE(ResourceId::typeFromExtension("translation.csv") == ResourceType::Localization);
        REQUIRE(ResourceId::typeFromExtension("locale.po") == ResourceType::Localization);
    }

    SECTION("Shader extensions are detected")
    {
        REQUIRE(ResourceId::typeFromExtension("shader.glsl") == ResourceType::Shader);
        REQUIRE(ResourceId::typeFromExtension("vertex.vert") == ResourceType::Shader);
        REQUIRE(ResourceId::typeFromExtension("fragment.frag") == ResourceType::Shader);
    }

    SECTION("Config extensions are detected")
    {
        REQUIRE(ResourceId::typeFromExtension("settings.cfg") == ResourceType::Config);
        REQUIRE(ResourceId::typeFromExtension("options.ini") == ResourceType::Config);
        REQUIRE(ResourceId::typeFromExtension("config.xml") == ResourceType::Config);
    }

    SECTION("Unknown extensions default to Data")
    {
        REQUIRE(ResourceId::typeFromExtension("file.xyz") == ResourceType::Data);
        REQUIRE(ResourceId::typeFromExtension("unknown.abc") == ResourceType::Data);
        REQUIRE(ResourceId::typeFromExtension("noextension") == ResourceType::Data);
    }

    SECTION("Case insensitive extension matching")
    {
        REQUIRE(ResourceId::typeFromExtension("IMAGE.PNG") == ResourceType::Texture);
        REQUIRE(ResourceId::typeFromExtension("Sound.WAV") == ResourceType::Audio);
        REQUIRE(ResourceId::typeFromExtension("Font.TTF") == ResourceType::Font);
    }

    SECTION("Path with directories works")
    {
        REQUIRE(ResourceId::typeFromExtension("assets/images/hero.png") == ResourceType::Texture);
        REQUIRE(ResourceId::typeFromExtension("/usr/share/fonts/arial.ttf") == ResourceType::Font);
        REQUIRE(ResourceId::typeFromExtension("C:\\sounds\\effect.wav") == ResourceType::Audio);
    }
}

TEST_CASE("ResourceId automatic type detection", "[resource][id][type]")
{
    SECTION("Constructor detects type from path")
    {
        ResourceId texture("sprites/hero.png");
        REQUIRE(texture.type() == ResourceType::Texture);

        ResourceId audio("sounds/bgm.ogg");
        REQUIRE(audio.type() == ResourceType::Audio);

        ResourceId font("fonts/arial.ttf");
        REQUIRE(font.type() == ResourceType::Font);
    }

    SECTION("Explicit type overrides detection")
    {
        // Even though extension is .png, we explicitly set it as Data
        ResourceId id("image.png", ResourceType::Data);
        REQUIRE(id.type() == ResourceType::Data);
    }
}

TEST_CASE("ResourceId hashing", "[resource][id][hash]")
{
    SECTION("Hash is computed for valid ID")
    {
        ResourceId id("test");
        REQUIRE(id.hash() != 0);
    }

    SECTION("Same ID produces same hash")
    {
        ResourceId id1("resource");
        ResourceId id2("resource");
        REQUIRE(id1.hash() == id2.hash());
    }

    SECTION("Different IDs produce different hashes")
    {
        ResourceId id1("resource1");
        ResourceId id2("resource2");
        REQUIRE(id1.hash() != id2.hash());
    }

    SECTION("Empty ID has zero hash")
    {
        ResourceId id;
        REQUIRE(id.hash() == 0);
    }

    SECTION("Type doesn't affect hash - only ID string")
    {
        ResourceId id1("data.txt", ResourceType::Data);
        ResourceId id2("data.txt", ResourceType::Config);
        // Hash is based on ID string, not type
        REQUIRE(id1.hash() == id2.hash());
    }
}

TEST_CASE("ResourceId comparison operators", "[resource][id][compare]")
{
    SECTION("Equality comparison")
    {
        ResourceId id1("test");
        ResourceId id2("test");
        ResourceId id3("other");

        REQUIRE(id1 == id2);
        REQUIRE_FALSE(id1 == id3);
    }

    SECTION("Inequality comparison")
    {
        ResourceId id1("test");
        ResourceId id2("other");

        REQUIRE(id1 != id2);
        REQUIRE_FALSE(id1 != id1);
    }

    SECTION("Less than comparison for ordering")
    {
        ResourceId id1("alpha");
        ResourceId id2("beta");

        REQUIRE(id1 < id2);
        REQUIRE_FALSE(id2 < id1);
        REQUIRE_FALSE(id1 < id1);
    }

    SECTION("Comparison is case sensitive")
    {
        ResourceId id1("Test");
        ResourceId id2("test");

        REQUIRE(id1 != id2);
    }

    SECTION("Empty IDs are equal")
    {
        ResourceId id1;
        ResourceId id2("");

        REQUIRE(id1 == id2);
    }
}

TEST_CASE("ResourceId as hash map key", "[resource][id][hashmap]")
{
    SECTION("Can be used in unordered_set")
    {
        std::unordered_set<ResourceId> idSet;

        ResourceId id1("res1");
        ResourceId id2("res2");
        ResourceId id3("res1"); // Duplicate

        idSet.insert(id1);
        idSet.insert(id2);
        idSet.insert(id3);

        // Only 2 unique elements
        REQUIRE(idSet.size() == 2);
        REQUIRE(idSet.count(id1) == 1);
        REQUIRE(idSet.count(id2) == 1);
    }

    SECTION("Can be used in unordered_map")
    {
        std::unordered_map<ResourceId, int> idMap;

        idMap[ResourceId("res1")] = 10;
        idMap[ResourceId("res2")] = 20;
        idMap[ResourceId("res1")] = 30; // Update

        REQUIRE(idMap.size() == 2);
        REQUIRE(idMap[ResourceId("res1")] == 30);
        REQUIRE(idMap[ResourceId("res2")] == 20);
    }
}

TEST_CASE("ResourceId std::hash specialization", "[resource][id][stdhash]")
{
    SECTION("std::hash works for ResourceId")
    {
        ResourceId id("test_hash");
        std::hash<ResourceId> hasher;

        size_t hashValue = hasher(id);
        REQUIRE(hashValue != 0);
        REQUIRE(hashValue == static_cast<size_t>(id.hash()));
    }

    SECTION("Consistent hashing")
    {
        ResourceId id("consistent");
        std::hash<ResourceId> hasher;

        size_t hash1 = hasher(id);
        size_t hash2 = hasher(id);

        REQUIRE(hash1 == hash2);
    }

    SECTION("Different IDs produce different std::hash values")
    {
        ResourceId id1("first");
        ResourceId id2("second");
        std::hash<ResourceId> hasher;

        REQUIRE(hasher(id1) != hasher(id2));
    }
}

TEST_CASE("ResourceId edge cases", "[resource][id][edge]")
{
    SECTION("Very long ID string")
    {
        std::string longId(1000, 'x');
        ResourceId id(longId);

        REQUIRE(id.id() == longId);
        REQUIRE(id.isValid());
        REQUIRE(id.hash() != 0);
    }

    SECTION("Special characters in ID")
    {
        ResourceId id("resource_with-special.chars@123");
        REQUIRE(id.id() == "resource_with-special.chars@123");
        REQUIRE(id.isValid());
    }

    SECTION("Unicode characters in ID")
    {
        ResourceId id("资源_リソース");
        REQUIRE(id.isValid());
        REQUIRE(id.hash() != 0);
    }

    SECTION("Path separators in ID")
    {
        ResourceId id1("assets/textures/hero.png");
        ResourceId id2("assets\\textures\\hero.png");

        REQUIRE(id1.isValid());
        REQUIRE(id2.isValid());
        REQUIRE(id1 != id2); // Different paths
    }

    SECTION("Multiple dots in filename")
    {
        ResourceId id("file.backup.old.png");
        REQUIRE(id.type() == ResourceType::Texture); // Uses last extension
    }

    SECTION("Dot at start (hidden files)")
    {
        ResourceId id(".hidden");
        REQUIRE(id.isValid());
    }
}

TEST_CASE("ResourceType enum values", "[resource][type]")
{
    SECTION("All ResourceType values are unique")
    {
        std::unordered_set<u8> typeValues;

        typeValues.insert(static_cast<u8>(ResourceType::Unknown));
        typeValues.insert(static_cast<u8>(ResourceType::Texture));
        typeValues.insert(static_cast<u8>(ResourceType::Audio));
        typeValues.insert(static_cast<u8>(ResourceType::Music));
        typeValues.insert(static_cast<u8>(ResourceType::Font));
        typeValues.insert(static_cast<u8>(ResourceType::Script));
        typeValues.insert(static_cast<u8>(ResourceType::Scene));
        typeValues.insert(static_cast<u8>(ResourceType::Localization));
        typeValues.insert(static_cast<u8>(ResourceType::Data));
        typeValues.insert(static_cast<u8>(ResourceType::Shader));
        typeValues.insert(static_cast<u8>(ResourceType::Config));

        // All values should be unique
        REQUIRE(typeValues.size() == 11);
    }

    SECTION("Unknown is zero")
    {
        REQUIRE(static_cast<u8>(ResourceType::Unknown) == 0);
    }
}

TEST_CASE("ResourceInfo structure", "[resource][info]")
{
    SECTION("Default construction")
    {
        ResourceInfo info;

        REQUIRE(info.size == 0);
        REQUIRE(info.compressedSize == 0);
        REQUIRE(info.checksum == 0);
        REQUIRE_FALSE(info.encrypted);
        REQUIRE_FALSE(info.compressed);
    }

    SECTION("Can store resource metadata")
    {
        ResourceInfo info;
        info.resourceId = ResourceId("test.png");
        info.size = 1024;
        info.compressedSize = 512;
        info.checksum = 0x12345678;
        info.encrypted = true;
        info.compressed = true;

        REQUIRE(info.resourceId.id() == "test.png");
        REQUIRE(info.size == 1024);
        REQUIRE(info.compressedSize == 512);
        REQUIRE(info.checksum == 0x12345678);
        REQUIRE(info.encrypted);
        REQUIRE(info.compressed);
    }
}
