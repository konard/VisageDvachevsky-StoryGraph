#include "NovelMind/editor/qt/panels/nm_issues_panel.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>

namespace NovelMind::editor::qt {

NMIssuesPanel::NMIssuesPanel(QWidget* parent) : NMDockPanel(tr("Problems"), parent) {
  setPanelId("Problems");
  auto* widget = new QWidget(this);
  auto* layout = new QVBoxLayout(widget);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->setSpacing(6);

  auto* label = new QLabel(tr("Problems"), widget);
  label->setObjectName("IssuesHeader");
  layout->addWidget(label);

  m_list = new QListWidget(widget);
  m_list->setSelectionMode(QAbstractItemView::SingleSelection);
  layout->addWidget(m_list, 1);

  setContentWidget(widget);
  setWindowTitle(tr("Problems"));

  connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
    if (!item) {
      return;
    }
    const QString file = item->data(Qt::UserRole).toString();
    const int line = item->data(Qt::UserRole + 1).toInt();
    emit issueActivated(file, line);
  });
}

void NMIssuesPanel::setIssues(const QList<NMScriptIssue>& issues) {
  m_list->clear();
  const auto& palette = NMStyleManager::instance().palette();
  for (const auto& issue : issues) {
    const QString fileName = QFileInfo(issue.file).fileName();
    auto* item =
        new QListWidgetItem(QString("%1(%2): %3").arg(fileName).arg(issue.line).arg(issue.message));
    QColor color = palette.textPrimary;
    if (issue.severity == "error") {
      color = QColor(220, 90, 90);
    } else if (issue.severity == "warning") {
      color = QColor(230, 180, 60);
    } else {
      color = palette.textSecondary;
    }
    item->setForeground(color);
    item->setData(Qt::UserRole, issue.file);
    item->setData(Qt::UserRole + 1, issue.line);
    m_list->addItem(item);
  }
}

} // namespace NovelMind::editor::qt
