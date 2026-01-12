#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "NovelMind/editor/qt/lazy_thumbnail_loader.hpp"
#include "NovelMind/editor/qt/performance_metrics.hpp"
#include "NovelMind/editor/qt/timeline_render_cache.hpp"

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QPixmap>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThread>

using namespace NovelMind::editor::qt;

namespace {

void ensureQtApp() {
  if (!QApplication::instance()) {
    static int argc = 1;
    static char arg0[] = "unit_tests";
    static char* argv[] = {arg0, nullptr};
    new QApplication(argc, argv);
  }
}

// Create a test image file
QString createTestImage(const QString& dir, const QString& name, int width = 100,
                        int height = 100) {
  QString path = dir + "/" + name;
  QImage image(width, height, QImage::Format_RGB32);
  image.fill(Qt::red);
  image.save(path, "PNG");
  return path;
}

} // namespace

// =============================================================================
// TimelineRenderCache Tests
// =============================================================================

TEST_CASE("TimelineRenderCache basic operations", "[cache][timeline]") {
  ensureQtApp();

  TimelineRenderCacheConfig config;
  config.maxMemoryBytes = 1024 * 1024; // 1 MB
  config.enableCache = true;

  TimelineRenderCache cache(config, nullptr);

  SECTION("Empty cache returns null pixmap") {
    RenderCacheKey key{0, 0, 100, 1.0f, 4};
    QPixmap result = cache.get(key, 1);
    CHECK(result.isNull());
  }

  SECTION("Can store and retrieve pixmap") {
    RenderCacheKey key{0, 0, 100, 1.0f, 4};
    QPixmap pixmap(100, 32);
    pixmap.fill(Qt::blue);

    cache.put(key, pixmap, 1);

    QPixmap result = cache.get(key, 1);
    CHECK(!result.isNull());
    CHECK(result.width() == 100);
    CHECK(result.height() == 32);
  }

  SECTION("Cache invalidation by data version") {
    RenderCacheKey key{0, 0, 100, 1.0f, 4};
    QPixmap pixmap(100, 32);
    pixmap.fill(Qt::blue);

    cache.put(key, pixmap, 1);

    // Different version should miss
    QPixmap result = cache.get(key, 2);
    CHECK(result.isNull());
  }

  SECTION("Track invalidation removes entries") {
    RenderCacheKey key0{0, 0, 100, 1.0f, 4};
    RenderCacheKey key1{1, 0, 100, 1.0f, 4};
    QPixmap pixmap(100, 32);
    pixmap.fill(Qt::blue);

    cache.put(key0, pixmap, 1);
    cache.put(key1, pixmap, 1);

    cache.invalidateTrack(0);

    CHECK(cache.get(key0, 1).isNull());
    CHECK(!cache.get(key1, 1).isNull());
  }

  SECTION("Frame range invalidation") {
    RenderCacheKey key1{0, 0, 50, 1.0f, 4};
    RenderCacheKey key2{0, 50, 100, 1.0f, 4};
    RenderCacheKey key3{0, 100, 150, 1.0f, 4};
    QPixmap pixmap(100, 32);
    pixmap.fill(Qt::blue);

    cache.put(key1, pixmap, 1);
    cache.put(key2, pixmap, 1);
    cache.put(key3, pixmap, 1);

    // Invalidate frames 40-60 - should affect key1 and key2
    cache.invalidateFrameRange(40, 60);

    CHECK(cache.get(key1, 1).isNull());  // 0-50 overlaps with 40-60
    CHECK(cache.get(key2, 1).isNull());  // 50-100 overlaps with 40-60
    CHECK(!cache.get(key3, 1).isNull()); // 100-150 doesn't overlap
  }

  SECTION("Statistics are tracked") {
    RenderCacheKey key{0, 0, 100, 1.0f, 4};
    QPixmap pixmap(100, 32);
    pixmap.fill(Qt::blue);

    cache.put(key, pixmap, 1);
    cache.get(key, 1); // Hit
    cache.get(key, 1); // Hit

    RenderCacheKey missing{1, 0, 100, 1.0f, 4};
    cache.get(missing, 1); // Miss

    auto stats = cache.getStats();
    CHECK(stats.entryCount == 1);
    CHECK(stats.hitCount == 2);
    CHECK(stats.missCount >= 1);
  }
}

TEST_CASE("TimelineRenderCache LRU eviction", "[cache][timeline]") {
  ensureQtApp();

  // Small cache that can only hold a few entries
  TimelineRenderCacheConfig config;
  config.maxMemoryBytes = 100 * 32 * 4 * 3; // ~3 entries of 100x32 pixels
  config.enableCache = true;

  TimelineRenderCache cache(config, nullptr);

  QPixmap pixmap(100, 32);
  pixmap.fill(Qt::blue);

  // Add entries
  for (int i = 0; i < 5; ++i) {
    RenderCacheKey key{i, 0, 100, 1.0f, 4};
    cache.put(key, pixmap, 1);
  }

  // First entries should be evicted
  RenderCacheKey key0{0, 0, 100, 1.0f, 4};
  RenderCacheKey key4{4, 0, 100, 1.0f, 4};

  CHECK(cache.get(key0, 1).isNull());  // Evicted
  CHECK(!cache.get(key4, 1).isNull()); // Still present

  auto stats = cache.getStats();
  CHECK(stats.evictionCount > 0);
}

TEST_CASE("TimelineRenderCache disable/enable", "[cache][timeline]") {
  ensureQtApp();

  TimelineRenderCacheConfig config;
  config.enableCache = false;

  TimelineRenderCache cache(config, nullptr);

  RenderCacheKey key{0, 0, 100, 1.0f, 4};
  QPixmap pixmap(100, 32);
  pixmap.fill(Qt::blue);

  cache.put(key, pixmap, 1);
  CHECK(cache.get(key, 1).isNull()); // Disabled cache always misses

  cache.setEnabled(true);
  cache.put(key, pixmap, 1);
  CHECK(!cache.get(key, 1).isNull()); // Now it works
}

// =============================================================================
// LazyThumbnailLoader Tests
// =============================================================================

TEST_CASE("LazyThumbnailLoader configuration", "[cache][asset]") {
  ensureQtApp();

  ThumbnailLoaderConfig config;
  config.maxConcurrentTasks = 4;
  config.maxCacheSizeKB = 100 * 1024;
  config.thumbnailSize = 128;
  config.queueHighWaterMark = 50;

  LazyThumbnailLoader loader(config, nullptr);

  auto resultConfig = loader.config();
  CHECK(resultConfig.maxConcurrentTasks == 4);
  CHECK(resultConfig.maxCacheSizeKB == 100 * 1024);
  CHECK(resultConfig.thumbnailSize == 128);
  CHECK(resultConfig.queueHighWaterMark == 50);
}

TEST_CASE("LazyThumbnailLoader cache operations", "[cache][asset]") {
  ensureQtApp();

  QTemporaryDir tempDir;
  REQUIRE(tempDir.isValid());

  ThumbnailLoaderConfig config;
  config.maxConcurrentTasks = 2;
  config.maxCacheSizeKB = 1024;
  config.thumbnailSize = 80;

  LazyThumbnailLoader loader(config, nullptr);

  SECTION("Initial cache is empty") {
    auto stats = loader.getStats();
    CHECK(stats.cachedCount == 0);
    CHECK(stats.pendingCount == 0);
    CHECK(stats.activeCount == 0);
  }

  SECTION("Request returns false for uncached file") {
    QString path = createTestImage(tempDir.path(), "test.png");
    bool cached = loader.requestThumbnail(path, QSize(80, 80));
    CHECK(!cached);
  }

  SECTION("Cancel clears pending requests") {
    // Request many thumbnails
    for (int i = 0; i < 10; ++i) {
      QString path = createTestImage(tempDir.path(), QString("test%1.png").arg(i));
      loader.requestThumbnail(path, QSize(80, 80));
    }

    loader.cancelPending();

    auto stats = loader.getStats();
    CHECK(stats.pendingCount == 0);
  }

  SECTION("Clear cache resets statistics") {
    loader.clearCache();

    auto stats = loader.getStats();
    CHECK(stats.cachedCount == 0);
    CHECK(stats.hitCount == 0);
    CHECK(stats.missCount == 0);
  }
}

TEST_CASE("LazyThumbnailLoader async loading", "[cache][asset]") {
  ensureQtApp();

  QTemporaryDir tempDir;
  REQUIRE(tempDir.isValid());

  ThumbnailLoaderConfig config;
  config.maxConcurrentTasks = 2;
  config.maxCacheSizeKB = 1024;

  LazyThumbnailLoader loader(config, nullptr);

  QString path = createTestImage(tempDir.path(), "async_test.png", 200, 200);

  QSignalSpy readySpy(&loader, &LazyThumbnailLoader::thumbnailReady);

  loader.requestThumbnail(path, QSize(80, 80));

  // Wait for thumbnail to load (up to 2 seconds)
  bool received = readySpy.wait(2000);
  CHECK(received);

  if (received) {
    QList<QVariant> args = readySpy.takeFirst();
    CHECK(args.at(0).toString() == path);
    CHECK(!args.at(1).value<QPixmap>().isNull());
  }
}

TEST_CASE("LazyThumbnailLoader safe shutdown", "[cache][asset]") {
  ensureQtApp();

  QTemporaryDir tempDir;
  REQUIRE(tempDir.isValid());

  // Create many test images
  for (int i = 0; i < 20; ++i) {
    createTestImage(tempDir.path(), QString("shutdown%1.png").arg(i));
  }

  {
    ThumbnailLoaderConfig config;
    config.maxConcurrentTasks = 4;

    LazyThumbnailLoader loader(config, nullptr);

    // Request many thumbnails
    for (int i = 0; i < 20; ++i) {
      QString path = tempDir.filePath(QString("shutdown%1.png").arg(i));
      loader.requestThumbnail(path, QSize(80, 80));
    }

    // Loader goes out of scope - should shutdown safely
  }

  // If we reach here without crash, shutdown was safe
  CHECK(true);
}

// =============================================================================
// PerformanceMetrics Tests
// =============================================================================

TEST_CASE("PerformanceMetrics timing", "[metrics]") {
  ensureQtApp();

  auto& metrics = PerformanceMetrics::instance();
  metrics.reset();
  metrics.setEnabled(true);

  SECTION("Record and retrieve timing") {
    metrics.recordTiming("TestMetric", 10.0);
    metrics.recordTiming("TestMetric", 20.0);
    metrics.recordTiming("TestMetric", 30.0);

    auto stats = metrics.getStats("TestMetric");
    CHECK(stats.sampleCount == 3);
    CHECK(stats.avgMs == Catch::Approx(20.0));
    CHECK(stats.minMs == Catch::Approx(10.0));
    CHECK(stats.maxMs == Catch::Approx(30.0));
  }

  SECTION("Record counts") {
    metrics.recordCount("ItemCount", 100);

    // Counts are stored separately from timing stats
    // Just verify no crash
    CHECK(true);
  }

  SECTION("Disabled metrics don't record") {
    metrics.reset();
    metrics.setEnabled(false);

    metrics.recordTiming("DisabledMetric", 10.0);

    auto stats = metrics.getStats("DisabledMetric");
    CHECK(stats.sampleCount == 0);
  }

  metrics.setEnabled(false);
}

TEST_CASE("ScopedTimer records timing", "[metrics]") {
  ensureQtApp();

  auto& metrics = PerformanceMetrics::instance();
  metrics.reset();
  metrics.setEnabled(true);

  {
    ScopedTimer timer("ScopedTest", true);
    QThread::msleep(10); // Sleep for 10ms
  }

  auto stats = metrics.getStats("ScopedTest");
  CHECK(stats.sampleCount == 1);
  CHECK(stats.avgMs >= 5.0); // At least 5ms (allowing for timing variance)

  metrics.setEnabled(false);
}
