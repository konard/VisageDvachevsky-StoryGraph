#pragma once

/**
 * @file story_graph_serialization.hpp
 * @brief Serialization and synchronization for Story Graph
 *
 * Handles bi-directional sync between Story Graph visual representation
 * and NMScript text files.
 */

#include <QObject>
#include <QString>
#include <QWidget>
#include <atomic>
#include <memory>

namespace NovelMind::editor::qt {

class NMStoryGraphPanel;

namespace serialization {

/**
 * @brief Sync graph data to NMScript files (Issue #82, #96)
 *
 * This operation runs asynchronously to avoid blocking the UI thread
 * during file I/O operations.
 *
 * @param panel The story graph panel
 * @param parent Parent widget for progress dialog
 */
void syncGraphToScript(NMStoryGraphPanel *panel, QWidget *parent);

/**
 * @brief Sync NMScript files to graph (Issue #127)
 *
 * Parses all .nms script files in the project and updates the graph
 * to match. This is the reverse operation of syncGraphToScript.
 *
 * @param panel The story graph panel
 * @param parent Parent widget for dialogs
 */
void syncScriptToGraph(NMStoryGraphPanel *panel, QWidget *parent);

// Helper struct for sync item data (Issue #96: async sync)
struct SyncItem {
  QString sceneId;
  QString scriptPath;
  QString speaker;
  QString dialogueText;
};

// Result struct for async sync operation
struct SyncResult {
  int nodesSynced = 0;
  int nodesSkipped = 0;
  QStringList syncErrors;
};

/**
 * @brief Worker class for async sync operation (Issue #96)
 *
 * This worker runs in a separate thread to perform file I/O operations
 * without blocking the UI thread.
 */
class SyncToScriptWorker : public QObject {
  Q_OBJECT

public:
  SyncToScriptWorker(QList<SyncItem> items,
                     std::shared_ptr<std::atomic<bool>> cancelled)
      : m_items(std::move(items)), m_cancelled(std::move(cancelled)) {}

public slots:
  void process();

signals:
  void progressUpdated(int current, int total);
  void finished(const SyncResult &result);

private:
  QList<SyncItem> m_items;
  std::shared_ptr<std::atomic<bool>> m_cancelled;
};

} // namespace serialization
} // namespace NovelMind::editor::qt
