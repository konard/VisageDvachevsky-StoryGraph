/**
 * @file test_benchmarks.cpp
 * @brief Performance benchmark tests for critical paths
 *
 * Benchmarks:
 * - Scene rendering with many objects
 * - Resource loading simulation
 * - Script execution overhead
 * - Memory usage patterns
 * - Search and filtering operations
 *
 * Related to Issue #179 - Performance testing coverage
 *
 * Note: Catch2 v3 supports BENCHMARK macro for micro-benchmarking
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "NovelMind/scene/scene_graph.hpp"
#include "NovelMind/vfs/memory_fs.hpp"
#include "NovelMind/renderer/renderer.hpp"
#include <vector>
#include <random>

using namespace NovelMind::scene;
using namespace NovelMind::vfs;
using namespace NovelMind;

// Mock renderer for benchmarking
class BenchmarkRenderer : public renderer::IRenderer {
public:
    Result<void> initialize(platform::IWindow& window) override {
        return Result<void>::ok();
    }
    void shutdown() override {}
    void beginFrame() override {}
    void endFrame() override {}
    void clear(const renderer::Color& color) override {}
    void setBlendMode(renderer::BlendMode mode) override {}
    void drawSprite(const renderer::Texture& texture, const renderer::Transform2D& transform,
                    const renderer::Color& tint) override {}
    void drawSprite(const renderer::Texture& texture, const renderer::Rect& sourceRect,
                    const renderer::Transform2D& transform, const renderer::Color& tint) override {}
    void drawRect(const renderer::Rect& rect, const renderer::Color& color) override {}
    void fillRect(const renderer::Rect& rect, const renderer::Color& color) override {}
    void drawText(const renderer::Font& font, const std::string& text, f32 x, f32 y,
                  const renderer::Color& color) override {}
    void setFade(f32 alpha, const renderer::Color& color) override {}
    [[nodiscard]] i32 getWidth() const override { return 1920; }
    [[nodiscard]] i32 getHeight() const override { return 1080; }
};

class TestObject : public SceneObjectBase {
public:
    explicit TestObject(const std::string& id)
        : SceneObjectBase(id, SceneObjectType::Custom) {}

    void render(renderer::IRenderer& renderer) override {
        // Minimal rendering work
    }
};

// =============================================================================
// Scene Graph Benchmarks
// =============================================================================

TEST_CASE("Benchmark: Scene rendering with many objects", "[benchmark][scene]")
{
    SceneGraph graph;
    BenchmarkRenderer renderer;

    // Setup scene with many objects
    for (int i = 0; i < 100; ++i) {
        auto obj = std::make_unique<TestObject>("obj_" + std::to_string(i));
        obj->setPosition(static_cast<f32>(i * 10), 100.0f);
        obj->setVisible(i % 2 == 0); // Half visible
        graph.addToLayer(LayerType::UI, std::move(obj));
    }

    BENCHMARK("Render 100 objects") {
        graph.render(renderer);
    };
}

TEST_CASE("Benchmark: Scene update with animations", "[benchmark][scene]")
{
    SceneGraph graph;

    // Setup objects with animations
    for (int i = 0; i < 50; ++i) {
        auto obj = std::make_unique<TestObject>("anim_" + std::to_string(i));
        obj->animatePosition(100.0f, 100.0f, 1.0f);
        obj->animateAlpha(0.5f, 1.0f);
        graph.addToLayer(LayerType::UI, std::move(obj));
    }

    BENCHMARK("Update 50 animated objects") {
        graph.update(0.016);
    };
}

TEST_CASE("Benchmark: Object search by tag", "[benchmark][scene]")
{
    SceneGraph graph;

    // Create objects with tags
    for (int i = 0; i < 200; ++i) {
        auto obj = std::make_unique<TestObject>("tagged_" + std::to_string(i));
        if (i % 5 == 0) obj->addTag("important");
        if (i % 3 == 0) obj->addTag("clickable");
        if (i % 7 == 0) obj->addTag("animated");
        graph.addToLayer(LayerType::UI, std::move(obj));
    }

    BENCHMARK("Find objects by tag (200 objects)") {
        return graph.findObjectsByTag("important");
    };

    BENCHMARK("Find objects by type (200 objects)") {
        return graph.findObjectsByType(SceneObjectType::Custom);
    };
}

TEST_CASE("Benchmark: Object creation and destruction", "[benchmark][scene]")
{
    SceneGraph graph;

    BENCHMARK("Create and add 100 objects") {
        for (int i = 0; i < 100; ++i) {
            auto obj = std::make_unique<TestObject>("temp_" + std::to_string(i));
            graph.addToLayer(LayerType::UI, std::move(obj));
        }
    };

    BENCHMARK("Remove 100 objects") {
        for (int i = 0; i < 100; ++i) {
            graph.removeFromLayer(LayerType::UI, "temp_" + std::to_string(i));
        }
    };
}

TEST_CASE("Benchmark: Scene serialization", "[benchmark][scene]")
{
    SceneGraph graph;

    // Setup complex scene
    graph.setSceneId("benchmark_scene");
    graph.showBackground("bg.png");

    for (int i = 0; i < 20; ++i) {
        auto obj = std::make_unique<TestObject>("obj_" + std::to_string(i));
        obj->setProperty("key1", "value1");
        obj->setProperty("key2", "value2");
        obj->setProperty("key3", "value3");
        obj->addTag("tag" + std::to_string(i % 5));
        graph.addToLayer(LayerType::UI, std::move(obj));
    }

    BENCHMARK("Save scene state") {
        return graph.saveState();
    };

    auto state = graph.saveState();

    BENCHMARK("Load scene state") {
        SceneGraph newGraph;
        newGraph.loadState(state);
    };
}

TEST_CASE("Benchmark: Layer z-order sorting", "[benchmark][scene]")
{
    Layer layer("Benchmark", LayerType::UI);

    // Add objects with random z-orders
    std::mt19937 rng(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<int> dist(-100, 100);

    for (int i = 0; i < 100; ++i) {
        auto obj = std::make_unique<TestObject>("z_" + std::to_string(i));
        obj->setZOrder(dist(rng));
        layer.addObject(std::move(obj));
    }

    BENCHMARK("Sort 100 objects by z-order") {
        layer.sortByZOrder();
    };
}

// =============================================================================
// VFS Benchmarks
// =============================================================================

TEST_CASE("Benchmark: VFS resource lookup", "[benchmark][vfs]")
{
    MemoryFileSystem vfs;

    // Add many resources
    for (int i = 0; i < 1000; ++i) {
        std::vector<u8> data(100, static_cast<u8>(i % 256));
        vfs.addResource("resource_" + std::to_string(i), data, ResourceType::Data);
    }

    BENCHMARK("Check resource existence (1000 resources)") {
        return vfs.exists("resource_500");
    };

    BENCHMARK("Get resource info") {
        return vfs.getInfo("resource_500");
    };

    BENCHMARK("List all resources") {
        return vfs.listResources();
    };

    BENCHMARK("List resources by type") {
        return vfs.listResources(ResourceType::Data);
    };
}

TEST_CASE("Benchmark: VFS resource reading", "[benchmark][vfs]")
{
    MemoryFileSystem vfs;

    // Add resources of various sizes
    std::vector<u8> smallData(1024);      // 1 KB
    std::vector<u8> mediumData(102400);   // 100 KB
    std::vector<u8> largeData(1048576);   // 1 MB

    vfs.addResource("small", smallData, ResourceType::Data);
    vfs.addResource("medium", mediumData, ResourceType::Data);
    vfs.addResource("large", largeData, ResourceType::Data);

    BENCHMARK("Read 1 KB resource") {
        return vfs.readFile("small");
    };

    BENCHMARK("Read 100 KB resource") {
        return vfs.readFile("medium");
    };

    BENCHMARK("Read 1 MB resource") {
        return vfs.readFile("large");
    };
}

TEST_CASE("Benchmark: VFS resource operations", "[benchmark][vfs]")
{
    MemoryFileSystem vfs;

    std::vector<u8> testData(1024);

    BENCHMARK("Add resource") {
        vfs.addResource("test", testData, ResourceType::Data);
    };

    vfs.addResource("removable", testData, ResourceType::Data);

    BENCHMARK("Remove resource") {
        vfs.removeResource("removable");
        vfs.addResource("removable", testData, ResourceType::Data);
    };
}

// =============================================================================
// Character and Dialogue Benchmarks
// =============================================================================

TEST_CASE("Benchmark: Character object operations", "[benchmark][character]")
{
    BENCHMARK("Create character object") {
        CharacterObject char1("char1", "sprite_id");
    };

    CharacterObject char1("char1", "sprite_id");

    BENCHMARK("Set character properties") {
        char1.setDisplayName("Alice");
        char1.setExpression("happy");
        char1.setPose("standing");
        char1.setHighlighted(true);
    };

    BENCHMARK("Serialize character") {
        return char1.saveState();
    };

    auto state = char1.saveState();

    BENCHMARK("Deserialize character") {
        CharacterObject char2("char2", "sprite_id");
        char2.loadState(state);
    };
}

TEST_CASE("Benchmark: Dialogue typewriter simulation", "[benchmark][dialogue]")
{
    DialogueUIObject dialogue("dlg1");
    dialogue.setText("This is a test message for benchmarking typewriter performance with a moderately long text.");
    dialogue.setTypewriterSpeed(100.0f);
    dialogue.startTypewriter();

    BENCHMARK("Typewriter update (60 frames)") {
        for (int i = 0; i < 60; ++i) {
            dialogue.update(1.0 / 60.0);
        }
    };
}

// =============================================================================
// Memory and Allocation Benchmarks
// =============================================================================

TEST_CASE("Benchmark: Object hierarchy creation", "[benchmark][hierarchy]")
{
    BENCHMARK("Create deep hierarchy (10 levels)") {
        auto root = std::make_unique<TestObject>("root");
        SceneObjectBase* current = root.get();

        for (int i = 0; i < 10; ++i) {
            auto child = std::make_unique<TestObject>("child_" + std::to_string(i));
            auto* childPtr = child.get();
            current->addChild(std::move(child));
            current = childPtr;
        }
    };

    BENCHMARK("Create wide hierarchy (50 children)") {
        auto root = std::make_unique<TestObject>("root");

        for (int i = 0; i < 50; ++i) {
            root->addChild(std::make_unique<TestObject>("child_" + std::to_string(i)));
        }
    };
}

TEST_CASE("Benchmark: Property system", "[benchmark][properties]")
{
    auto obj = std::make_unique<TestObject>("prop_test");

    BENCHMARK("Set 10 properties") {
        for (int i = 0; i < 10; ++i) {
            obj->setProperty("key" + std::to_string(i), "value" + std::to_string(i));
        }
    };

    // Setup properties
    for (int i = 0; i < 100; ++i) {
        obj->setProperty("key" + std::to_string(i), "value" + std::to_string(i));
    }

    BENCHMARK("Get property (100 properties)") {
        return obj->getProperty("key50");
    };
}

// =============================================================================
// Real-world Scenario Benchmarks
// =============================================================================

TEST_CASE("Benchmark: Typical VN scene frame", "[benchmark][integration]")
{
    SceneGraph graph;
    BenchmarkRenderer renderer;

    // Setup typical VN scene
    graph.showBackground("bg.png");
    auto* char1 = graph.showCharacter("alice", "alice", CharacterObject::Position::Left);
    auto* char2 = graph.showCharacter("bob", "bob", CharacterObject::Position::Right);
    graph.showDialogue("Alice", "This is a typical dialogue line in a visual novel.");

    BENCHMARK("Full frame: update + render") {
        graph.update(0.016);
        graph.render(renderer);
    };
}

TEST_CASE("Benchmark: Scene transition", "[benchmark][integration]")
{
    SceneGraph graph;

    // Setup initial scene
    graph.setSceneId("scene1");
    for (int i = 0; i < 10; ++i) {
        auto obj = std::make_unique<TestObject>("obj_" + std::to_string(i));
        graph.addToLayer(LayerType::UI, std::move(obj));
    }

    BENCHMARK("Clear scene and setup new scene") {
        // Save state
        auto state = graph.saveState();

        // Clear
        graph.clear();

        // Setup new scene
        graph.setSceneId("scene2");
        for (int i = 0; i < 10; ++i) {
            auto obj = std::make_unique<TestObject>("new_" + std::to_string(i));
            graph.addToLayer(LayerType::UI, std::move(obj));
        }

        // Restore original state for next iteration
        graph.loadState(state);
    };
}

// Note: These benchmarks provide baseline performance metrics.
// For production performance tuning, use a dedicated profiler like:
// - perf (Linux)
// - Instruments (macOS)
// - Visual Studio Profiler (Windows)
// - Tracy Profiler (cross-platform)
