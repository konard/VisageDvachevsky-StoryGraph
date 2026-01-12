#include "NovelMind/editor/qt/panels/inspector_ui.hpp"
#include "NovelMind/editor/qt/panels/nm_inspector_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_inspector_panel_group.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"

#include <QFrame>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

InspectorUI::InspectorUI(NMInspectorPanel* panel) : m_panel(panel) {}

QWidget* InspectorUI::setupContent(QWidget* parent) {
  auto* container = new QWidget(parent);
  auto* containerLayout = new QVBoxLayout(container);
  containerLayout->setContentsMargins(0, 0, 0, 0);
  containerLayout->setSpacing(0);

  // Header
  m_headerLabel = new QLabel(container);
  m_headerLabel->setObjectName("InspectorHeader");
  m_headerLabel->setWordWrap(true);
  m_headerLabel->setTextFormat(Qt::RichText);
  m_headerLabel->setMargin(8);
  m_headerLabel->hide();
  containerLayout->addWidget(m_headerLabel);

  // Scroll area
  m_scrollArea = new QScrollArea(container);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setFrameShape(QFrame::NoFrame);

  m_scrollContent = new QWidget(m_scrollArea);
  m_mainLayout = new QVBoxLayout(m_scrollContent);
  m_mainLayout->setContentsMargins(0, 0, 0, 0);
  m_mainLayout->setSpacing(8);
  m_mainLayout->setAlignment(Qt::AlignTop);

  m_scrollArea->setWidget(m_scrollContent);
  containerLayout->addWidget(m_scrollArea, 1);

  // No selection label
  m_noSelectionLabel =
      new QLabel(QObject::tr("Select an object to view its properties"), container);
  m_noSelectionLabel->setObjectName("InspectorEmptyState");
  m_noSelectionLabel->setAlignment(Qt::AlignCenter);
  m_noSelectionLabel->setWordWrap(true);

  const auto& palette = NMStyleManager::instance().palette();
  m_noSelectionLabel->setStyleSheet(
      QString("color: %1; padding: 20px;")
          .arg(NMStyleManager::colorToStyleString(palette.textSecondary)));

  m_mainLayout->addWidget(m_noSelectionLabel);

  return container;
}

NMPropertyGroup* InspectorUI::addGroup(const QString& title, QWidget* scrollContent,
                                       QVBoxLayout* mainLayout, QList<NMPropertyGroup*>& groups) {
  auto* group = new NMPropertyGroup(title, scrollContent);
  mainLayout->addWidget(group);
  groups.append(group);
  return group;
}

void InspectorUI::showNoSelection(QLabel* headerLabel, QLabel* noSelectionLabel) {
  if (headerLabel) {
    headerLabel->hide();
  }
  if (noSelectionLabel) {
    noSelectionLabel->show();
  }
}

void InspectorUI::clearGroups(QVBoxLayout* mainLayout, QList<NMPropertyGroup*>& groups,
                              QLabel* headerLabel) {
  // Remove all groups
  for (auto* group : groups) {
    mainLayout->removeWidget(group);
    group->deleteLater();
  }
  groups.clear();

  if (headerLabel) {
    headerLabel->clear();
  }
}

void InspectorUI::setHeader(QLabel* headerLabel, const QString& text) {
  if (headerLabel) {
    headerLabel->setText(text);
    headerLabel->show();
  }
}

} // namespace NovelMind::editor::qt
