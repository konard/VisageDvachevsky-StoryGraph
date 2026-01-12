#include <catch2/catch_test_macros.hpp>

#include "NovelMind/editor/qt/panels/nm_asset_browser_panel.hpp"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QListView>
#include <QTemporaryDir>

using namespace NovelMind::editor::qt;

namespace {

void ensureQtApp() {
  if (!QApplication::instance()) {
    static int argc = 1;
    static char arg0[] = "integration_tests";
    static char* argv[] = {arg0, nullptr};
    new QApplication(argc, argv);
  }
}

} // namespace

TEST_CASE("Asset Browser shows imported files with uppercase extensions", "[asset_browser]") {
  ensureQtApp();

  QTemporaryDir tempDir;
  REQUIRE(tempDir.isValid());

  const QString filePath = tempDir.filePath("TestImage.PNG");
  QFile file(filePath);
  REQUIRE(file.open(QIODevice::WriteOnly));
  file.write("x");
  file.close();

  NMAssetBrowserPanel panel(nullptr);
  panel.setRootPath(tempDir.path());
  panel.refresh();

  auto* listView = panel.findChild<QListView*>("AssetBrowserListView");
  REQUIRE(listView != nullptr);
  auto* model = listView->model();
  REQUIRE(model != nullptr);

  QElapsedTimer timer;
  timer.start();
  while (model->rowCount(listView->rootIndex()) == 0 && timer.elapsed() < 2000) {
    QCoreApplication::processEvents();
  }

  REQUIRE(model->rowCount(listView->rootIndex()) == 1);
  const QModelIndex idx = model->index(0, 0, listView->rootIndex());
  CHECK(idx.data(Qt::DisplayRole).toString() == "TestImage.PNG");
}
