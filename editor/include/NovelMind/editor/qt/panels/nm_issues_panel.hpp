#pragma once

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include <QListWidget>

namespace NovelMind::editor::qt {

struct NMScriptIssue {
  QString file;
  int line = 0;
  QString message;
  QString severity; // "error", "warning", "info"
};

class NMIssuesPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMIssuesPanel(QWidget* parent = nullptr);

  void setIssues(const QList<NMScriptIssue>& issues);

signals:
  void issueActivated(const QString& file, int line);

private:
  QListWidget* m_list = nullptr;
};

} // namespace NovelMind::editor::qt
