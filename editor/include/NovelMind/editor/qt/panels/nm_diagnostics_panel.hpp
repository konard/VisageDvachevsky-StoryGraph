#pragma once

/**
 * @file nm_diagnostics_panel.hpp
 * @brief Diagnostics panel for errors, warnings, and issues with rich info
 * support
 */

#include "NovelMind/editor/error_reporter.hpp"
#include "NovelMind/editor/qt/nm_dock_panel.hpp"

#include <QToolBar>
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QToolBar;
class QPushButton;

namespace NovelMind::editor::qt {

class NMDiagnosticsPanel : public NMDockPanel, public ::NovelMind::editor::IDiagnosticListener {
  Q_OBJECT

public:
  explicit NMDiagnosticsPanel(QWidget* parent = nullptr);
  ~NMDiagnosticsPanel() override;

  void onInitialize() override;
  void onShutdown() override;
  void onUpdate(double deltaTime) override;

  // IDiagnosticListener interface
  void onDiagnosticAdded(const ::NovelMind::editor::Diagnostic& diagnostic) override;
  void onAllDiagnosticsCleared() override;

  // Legacy methods for backward compatibility
  void addDiagnostic(const QString& type, const QString& message, const QString& file = QString(),
                     int line = -1);
  void addDiagnosticWithLocation(const QString& type, const QString& message,
                                 const QString& location);
  void clearDiagnostics();

signals:
  void diagnosticActivated(const QString& location);

private:
  void setupUI();
  void applyTypeFilter();
  void addDiagnosticToTree(const ::NovelMind::editor::Diagnostic& diag);
  QTreeWidgetItem* createDiagnosticItem(const ::NovelMind::editor::Diagnostic& diag);
  void createRelatedInfoItems(QTreeWidgetItem* parent, const ::NovelMind::editor::Diagnostic& diag);
  void createSuggestionItems(QTreeWidgetItem* parent, const ::NovelMind::editor::Diagnostic& diag);
  QString diagnosticTypeString(const ::NovelMind::editor::Diagnostic& diag) const;
  QColor severityColor(::NovelMind::editor::DiagnosticSeverity severity) const;
  QString locationString(const ::NovelMind::editor::SourceLocation& loc) const;

  QTreeWidget* m_diagnosticsTree = nullptr;
  QToolBar* m_toolbar = nullptr;
  QString m_activeFilter;
};

} // namespace NovelMind::editor::qt
