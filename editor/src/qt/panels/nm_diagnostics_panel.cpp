#include "NovelMind/editor/qt/panels/nm_diagnostics_panel.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/qt_event_bus.hpp"

#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QPushButton>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

NMDiagnosticsPanel::NMDiagnosticsPanel(QWidget* parent) : NMDockPanel("Diagnostics", parent) {}

NMDiagnosticsPanel::~NMDiagnosticsPanel() {
  NovelMind::editor::ErrorReporter::instance().removeListener(this);
}

void NMDiagnosticsPanel::onInitialize() {
  setupUI();
  // Register as listener to ErrorReporter
  NovelMind::editor::ErrorReporter::instance().addListener(this);

  // Load existing diagnostics
  auto diagnostics = NovelMind::editor::ErrorReporter::instance().getAllDiagnostics();
  for (const auto& diag : diagnostics) {
    addDiagnosticToTree(diag);
  }
}

void NMDiagnosticsPanel::onShutdown() {
  NovelMind::editor::ErrorReporter::instance().removeListener(this);
}

void NMDiagnosticsPanel::setupUI() {
  QVBoxLayout* layout = new QVBoxLayout(contentWidget());
  layout->setContentsMargins(0, 0, 0, 0);

  // Get icon manager instance
  auto& iconMgr = NMIconManager::instance();

  m_toolbar = new QToolBar(contentWidget());
  auto* clearButton = new QPushButton("Clear All", m_toolbar);
  clearButton->setIcon(iconMgr.getIcon("delete", 16));
  m_toolbar->addWidget(clearButton);
  m_toolbar->addSeparator();
  auto* allButton = new QPushButton("All", m_toolbar);
  allButton->setIcon(iconMgr.getIcon("filter", 16));
  auto* errorButton = new QPushButton("Errors", m_toolbar);
  errorButton->setIcon(iconMgr.getIcon("status-error", 16));
  auto* warningButton = new QPushButton("Warnings", m_toolbar);
  warningButton->setIcon(iconMgr.getIcon("status-warning", 16));
  auto* infoButton = new QPushButton("Info", m_toolbar);
  infoButton->setIcon(iconMgr.getIcon("status-info", 16));
  m_toolbar->addWidget(allButton);
  m_toolbar->addWidget(errorButton);
  m_toolbar->addWidget(warningButton);
  m_toolbar->addWidget(infoButton);
  layout->addWidget(m_toolbar);

  m_diagnosticsTree = new QTreeWidget(contentWidget());
  m_diagnosticsTree->setHeaderLabels({"Type", "Message", "File", "Line"});
  m_diagnosticsTree->setContextMenuPolicy(Qt::CustomContextMenu);
  m_diagnosticsTree->setWordWrap(true);

  // Set column widths - make Message and File columns much wider
  m_diagnosticsTree->setColumnWidth(0, 80);  // Type
  m_diagnosticsTree->setColumnWidth(1, 500); // Message - much wider
  m_diagnosticsTree->setColumnWidth(2, 300); // File - wider
  m_diagnosticsTree->setColumnWidth(3, 60);  // Line

  // Enable text wrapping in items
  m_diagnosticsTree->setTextElideMode(Qt::ElideNone);

  layout->addWidget(m_diagnosticsTree, 1);

  // Enable double-click to navigate
  connect(m_diagnosticsTree, &QTreeWidget::itemDoubleClicked, this,
          [this](QTreeWidgetItem* item, int column) {
            Q_UNUSED(column);
            if (!item) {
              return;
            }
            // Get location from item data
            QString location = item->data(0, Qt::UserRole).toString();
            if (!location.isEmpty()) {
              emit diagnosticActivated(location);
            }
          });

  // Context menu for copying suggestions
  connect(m_diagnosticsTree, &QTreeWidget::customContextMenuRequested, this,
          [this](const QPoint& pos) {
            QTreeWidgetItem* item = m_diagnosticsTree->itemAt(pos);
            if (!item) {
              return;
            }

            // Check if this is a suggestion item
            QString itemType = item->data(0, Qt::UserRole + 1).toString();
            if (itemType == "suggestion") {
              QMenu menu(m_diagnosticsTree);
              QAction* copyAction = menu.addAction("Copy Suggestion");
              connect(copyAction, &QAction::triggered, this, [item]() {
                QString suggestionText = item->data(0, Qt::UserRole + 2).toString();
                QApplication::clipboard()->setText(suggestionText);
              });
              menu.exec(m_diagnosticsTree->viewport()->mapToGlobal(pos));
            }
          });

  connect(clearButton, &QPushButton::clicked, this, &NMDiagnosticsPanel::clearDiagnostics);
  connect(allButton, &QPushButton::clicked, this, [this]() {
    m_activeFilter.clear();
    applyTypeFilter();
  });
  connect(errorButton, &QPushButton::clicked, this, [this]() {
    m_activeFilter = "Error";
    applyTypeFilter();
  });
  connect(warningButton, &QPushButton::clicked, this, [this]() {
    m_activeFilter = "Warning";
    applyTypeFilter();
  });
  connect(infoButton, &QPushButton::clicked, this, [this]() {
    m_activeFilter = "Info";
    applyTypeFilter();
  });

  // Keep backward compatibility with QtEventBus
  auto& bus = QtEventBus::instance();
  connect(&bus, &QtEventBus::errorOccurred, this,
          [this](const QString& message, const QString& details) {
            QString msg = message;
            if (!details.isEmpty()) {
              msg = message + " (" + details + ")";
            }
            addDiagnostic("Error", msg);
          });
  connect(&bus, &QtEventBus::logMessage, this,
          [this](const QString& message, const QString& source, int level) {
            Q_UNUSED(level);
            addDiagnostic("Info", source.isEmpty() ? message : source + ": " + message);
          });
}

void NMDiagnosticsPanel::onUpdate([[maybe_unused]] double deltaTime) {}

// IDiagnosticListener interface
void NMDiagnosticsPanel::onDiagnosticAdded(const NovelMind::editor::Diagnostic& diagnostic) {
  addDiagnosticToTree(diagnostic);
}

void NMDiagnosticsPanel::onAllDiagnosticsCleared() {
  clearDiagnostics();
}

// Helper methods
void NMDiagnosticsPanel::addDiagnosticToTree(const NovelMind::editor::Diagnostic& diag) {
  if (!m_diagnosticsTree) {
    return;
  }

  QTreeWidgetItem* item = createDiagnosticItem(diag);
  if (!item) {
    return;
  }

  // Add related info as children
  createRelatedInfoItems(item, diag);

  // Add suggestions as children
  createSuggestionItems(item, diag);

  applyTypeFilter();
}

QTreeWidgetItem*
NMDiagnosticsPanel::createDiagnosticItem(const NovelMind::editor::Diagnostic& diag) {
  auto* item = new QTreeWidgetItem(m_diagnosticsTree);

  QString typeStr = diagnosticTypeString(diag);
  item->setText(0, typeStr);
  item->setText(1, QString::fromStdString(diag.message));

  // Set file and line from location
  if (diag.location.isValid()) {
    item->setText(2, QString::fromStdString(diag.location.file));
    item->setText(3, QString::number(diag.location.line));

    // Store location for navigation
    QString location = locationString(diag.location);
    item->setData(0, Qt::UserRole, location);
  } else {
    item->setText(2, QString("-"));
    item->setText(3, QString("-"));
  }

  // Set severity color
  QColor color = severityColor(diag.severity);
  item->setForeground(0, QBrush(color));

  // Store item type for filtering
  item->setData(0, Qt::UserRole + 1, "diagnostic");

  return item;
}

void NMDiagnosticsPanel::createRelatedInfoItems(QTreeWidgetItem* parent,
                                                const NovelMind::editor::Diagnostic& diag) {
  if (diag.relatedInfo.empty()) {
    return;
  }

  for (const auto& related : diag.relatedInfo) {
    auto* relatedItem = new QTreeWidgetItem(parent);
    relatedItem->setText(0, "Related");
    relatedItem->setText(1, QString::fromStdString(related.message));

    if (related.location.isValid()) {
      relatedItem->setText(2, QString::fromStdString(related.location.file));
      relatedItem->setText(3, QString::number(related.location.line));

      // Store location for navigation
      QString location = locationString(related.location);
      relatedItem->setData(0, Qt::UserRole, location);
    } else {
      relatedItem->setText(2, QString("-"));
      relatedItem->setText(3, QString("-"));
    }

    // Style related items with a lighter color
    relatedItem->setForeground(0, QBrush(QColor("#9e9e9e")));
    relatedItem->setData(0, Qt::UserRole + 1, "related");
  }
}

void NMDiagnosticsPanel::createSuggestionItems(QTreeWidgetItem* parent,
                                               const NovelMind::editor::Diagnostic& diag) {
  if (diag.fixes.empty()) {
    return;
  }

  for (const auto& fix : diag.fixes) {
    auto* suggestionItem = new QTreeWidgetItem(parent);
    suggestionItem->setText(0, "Suggestion");
    suggestionItem->setText(1, QString::fromStdString(fix.title));

    if (!fix.description.empty()) {
      suggestionItem->setToolTip(1, QString::fromStdString(fix.description));
    }

    if (fix.range.isValid()) {
      suggestionItem->setText(2, QString::fromStdString(fix.range.file));
      suggestionItem->setText(3, QString::number(fix.range.line));
    } else {
      suggestionItem->setText(2, QString("-"));
      suggestionItem->setText(3, QString("-"));
    }

    // Style suggestion items with a green color
    suggestionItem->setForeground(0, QBrush(QColor("#4caf50")));

    // Store suggestion info for copying
    suggestionItem->setData(0, Qt::UserRole + 1, "suggestion");
    suggestionItem->setData(0, Qt::UserRole + 2, QString::fromStdString(fix.replacementText));
  }
}

QString NMDiagnosticsPanel::diagnosticTypeString(const NovelMind::editor::Diagnostic& diag) const {
  switch (diag.severity) {
  case NovelMind::editor::DiagnosticSeverity::Fatal:
    return "Error";
  case NovelMind::editor::DiagnosticSeverity::Error:
    return "Error";
  case NovelMind::editor::DiagnosticSeverity::Warning:
    return "Warning";
  case NovelMind::editor::DiagnosticSeverity::Info:
    return "Info";
  case NovelMind::editor::DiagnosticSeverity::Hint:
    return "Hint";
  default:
    return "Unknown";
  }
}

QColor NMDiagnosticsPanel::severityColor(NovelMind::editor::DiagnosticSeverity severity) const {
  switch (severity) {
  case NovelMind::editor::DiagnosticSeverity::Fatal:
    return QColor("#d32f2f"); // Dark red
  case NovelMind::editor::DiagnosticSeverity::Error:
    return QColor("#f44336"); // Red
  case NovelMind::editor::DiagnosticSeverity::Warning:
    return QColor("#ff9800"); // Orange
  case NovelMind::editor::DiagnosticSeverity::Info:
    return QColor("#64b5f6"); // Blue
  case NovelMind::editor::DiagnosticSeverity::Hint:
    return QColor("#9e9e9e"); // Grey
  default:
    return QColor("#9e9e9e");
  }
}

QString NMDiagnosticsPanel::locationString(const NovelMind::editor::SourceLocation& loc) const {
  if (!loc.isValid()) {
    return QString();
  }

  QString location = QString("Script:%1:%2:%3")
                         .arg(QString::fromStdString(loc.file))
                         .arg(loc.line)
                         .arg(loc.column);
  return location;
}

// Legacy methods for backward compatibility
void NMDiagnosticsPanel::addDiagnostic(const QString& type, const QString& message,
                                       const QString& file, int line) {
  if (!m_diagnosticsTree) {
    return;
  }
  auto* item = new QTreeWidgetItem(m_diagnosticsTree);
  item->setText(0, type);
  item->setText(1, message);
  item->setText(2, file);
  item->setText(3, line >= 0 ? QString::number(line) : QString("-"));

  // Store location string for navigation
  if (!file.isEmpty()) {
    QString location;
    if (line > 0) {
      location = QString("Script:%1:%2").arg(file).arg(line);
    } else {
      location = QString("Script:%1").arg(file);
    }
    item->setData(0, Qt::UserRole, location);
  }

  QColor color;
  if (type.compare("Error", Qt::CaseInsensitive) == 0) {
    color = QColor("#f44336");
  } else if (type.compare("Warning", Qt::CaseInsensitive) == 0) {
    color = QColor("#ff9800");
  } else {
    color = QColor("#64b5f6");
  }
  item->setForeground(0, QBrush(color));
  applyTypeFilter();
}

void NMDiagnosticsPanel::addDiagnosticWithLocation(const QString& type, const QString& message,
                                                   const QString& location) {
  if (!m_diagnosticsTree) {
    return;
  }
  auto* item = new QTreeWidgetItem(m_diagnosticsTree);
  item->setText(0, type);
  item->setText(1, message);

  // Parse location to populate file/line columns if it's a Script location
  if (location.startsWith("Script:", Qt::CaseInsensitive)) {
    QStringList parts = location.split(':');
    if (parts.size() >= 2) {
      item->setText(2, parts[1]);
    }
    if (parts.size() >= 3) {
      item->setText(3, parts[2]);
    }
  } else if (location.startsWith("StoryGraph:", Qt::CaseInsensitive)) {
    QStringList parts = location.split(':');
    if (parts.size() >= 2) {
      item->setText(2, QString("Node: %1").arg(parts[1]));
    }
    item->setText(3, QString("-"));
  } else if (location.startsWith("Asset:", Qt::CaseInsensitive)) {
    QStringList parts = location.split(':');
    if (parts.size() >= 2) {
      item->setText(2, parts[1]);
    }
    item->setText(3, QString("-"));
  } else if (location.startsWith("File:", Qt::CaseInsensitive)) {
    QStringList parts = location.split(':');
    if (parts.size() >= 2) {
      item->setText(2, parts[1]);
    }
    if (parts.size() >= 3) {
      item->setText(3, parts[2]);
    } else {
      item->setText(3, QString("-"));
    }
  }

  // Store the full location string
  item->setData(0, Qt::UserRole, location);

  QColor color;
  if (type.compare("Error", Qt::CaseInsensitive) == 0) {
    color = QColor("#f44336");
  } else if (type.compare("Warning", Qt::CaseInsensitive) == 0) {
    color = QColor("#ff9800");
  } else {
    color = QColor("#64b5f6");
  }
  item->setForeground(0, QBrush(color));
  applyTypeFilter();
}

void NMDiagnosticsPanel::clearDiagnostics() {
  if (m_diagnosticsTree) {
    m_diagnosticsTree->clear();
  }
}

void NMDiagnosticsPanel::applyTypeFilter() {
  if (!m_diagnosticsTree) {
    return;
  }
  for (int i = 0; i < m_diagnosticsTree->topLevelItemCount(); ++i) {
    auto* item = m_diagnosticsTree->topLevelItem(i);
    const bool visible =
        m_activeFilter.isEmpty() || item->text(0).compare(m_activeFilter, Qt::CaseInsensitive) == 0;
    item->setHidden(!visible);
  }
}

} // namespace NovelMind::editor::qt
