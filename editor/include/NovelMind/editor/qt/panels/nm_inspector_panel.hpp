#pragma once

/**
 * @file nm_inspector_panel.hpp
 * @brief Inspector panel for viewing and editing object properties
 *
 * Displays properties of the currently selected object:
 * - Property groups (collapsible)
 * - Various property types (text, number, color, etc.)
 * - Read-only view (Phase 1)
 * - Editable properties (Phase 2+)
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include <QHash>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

/**
 * @brief Property editor widget types
 */
enum class NMPropertyType {
  String,
  MultiLine,
  Integer,
  Float,
  Boolean,
  Color,
  Enum,
  Asset,
  Curve,   // Animation curve editor
  Vector2, // 2D vector (x, y)
  Vector3, // 3D vector (x, y, z)
  Range    // Min/max range slider
};

/**
 * @brief Property metadata for auto-generation and validation
 */
struct NMPropertyMetadata {
  QString displayName;
  QString tooltip;
  QString category;
  bool readOnly = false;
  bool hidden = false;
  QVariant defaultValue;
  QVariant minValue;
  QVariant maxValue;
  QString validationRegex;
  QStringList enumOptions;
};

/**
 * @brief A collapsible group box for property categories
 */
class NMPropertyGroup : public QWidget {
  Q_OBJECT

public:
  explicit NMPropertyGroup(const QString &title, QWidget *parent = nullptr);

  void setExpanded(bool expanded);
  [[nodiscard]] bool isExpanded() const { return m_expanded; }

  void addProperty(const QString &name, const QString &value);
  void addProperty(const QString &name, QWidget *widget);

  /**
   * @brief Add an editable property
   */
  QWidget *addEditableProperty(const QString &propertyName,
                               const QString &label, NMPropertyType type,
                               const QString &currentValue,
                               const QStringList &enumValues = {});
  QWidget *addEditableProperty(const QString &name, NMPropertyType type,
                               const QString &currentValue,
                               const QStringList &enumValues = {});

  /**
   * @brief Add an editable property with metadata
   */
  QWidget *addEditablePropertyWithMetadata(const QString &propertyName,
                                           NMPropertyType type,
                                           const QString &currentValue,
                                           const NMPropertyMetadata &metadata);

  /**
   * @brief Add a reset button for a property
   */
  void addResetButton(const QString &propertyName,
                      const QVariant &defaultValue);

  void clearProperties();

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

signals:
  void propertyValueChanged(const QString &propertyName,
                            const QString &newValue);

private slots:
  void onHeaderClicked();
  void onPropertyEdited();

private:
  QWidget *m_header = nullptr;
  QWidget *m_content = nullptr;
  QVBoxLayout *m_contentLayout = nullptr;
  QLabel *m_expandIcon = nullptr;
  bool m_expanded = true;
};

/**
 * @brief Inspector panel for property editing
 */
class NMInspectorPanel : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMInspectorPanel(QWidget *parent = nullptr);
  ~NMInspectorPanel() override;

  void onInitialize() override;
  void onUpdate(double deltaTime) override;

  /**
   * @brief Clear all properties
   */
  void clear();

  /**
   * @brief Show properties for an object
   * @param objectType Type of the object (for header display)
   * @param objectId ID of the object
   * @param editable Whether properties should be editable
   */
  void inspectObject(const QString &objectType, const QString &objectId,
                     bool editable = true);
  void inspectSceneObject(class NMSceneObject *object, bool editable = true);
  void inspectStoryGraphNode(class NMGraphNodeItem *node, bool editable = true);

  /**
   * @brief Inspect multiple objects for multi-edit
   */
  void inspectMultipleObjects(const QList<class NMSceneObject *> &objects,
                              bool editable = true);

  /**
   * @brief Copy all visible properties to clipboard
   */
  void copyProperties();

  /**
   * @brief Paste properties from clipboard
   */
  void pasteProperties();

  /**
   * @brief Reset property to default value
   */
  void resetProperty(const QString &propertyName);

  /**
   * @brief Reset all properties to default values
   */
  void resetAllProperties();

  void updatePropertyValue(const QString &propertyName,
                           const QString &newValue);
  [[nodiscard]] QString currentObjectId() const { return m_currentObjectId; }
  [[nodiscard]] QStringList currentObjectIds() const {
    return m_currentObjectIds;
  }
  [[nodiscard]] bool isMultiEditMode() const { return m_multiEditMode; }

  /**
   * @brief Add a property group
   */
  NMPropertyGroup *addGroup(const QString &title);

  /**
   * @brief Show "nothing selected" message
   */
  void showNoSelection();

  /**
   * @brief Enable or disable edit mode
   */
  void setEditMode(bool enabled) { m_editMode = enabled; }
  [[nodiscard]] bool editMode() const { return m_editMode; }

signals:
  void propertyChanged(const QString &objectId, const QString &propertyName,
                       const QString &newValue);
  /// ERR-1 fix: Signal emitted when property setting fails
  void propertyError(const QString &propertyName, const QString &errorMessage);

private slots:
  void onGroupPropertyChanged(const QString &propertyName,
                              const QString &newValue);

private:
  void setupContent();
  void trackPropertyWidget(const QString &propertyName, QWidget *widget);

  QScrollArea *m_scrollArea = nullptr;
  QWidget *m_scrollContent = nullptr;
  QVBoxLayout *m_mainLayout = nullptr;
  QLabel *m_headerLabel = nullptr;
  QLabel *m_noSelectionLabel = nullptr;
  QList<NMPropertyGroup *> m_groups;
  QHash<QString, QWidget *> m_propertyWidgets;
  QHash<QString, NMPropertyMetadata> m_propertyMetadata;
  QHash<QString, QVariant> m_defaultValues;
  QString m_currentObjectId;
  QStringList m_currentObjectIds;
  bool m_editMode = true;
  bool m_multiEditMode = false;
  QHash<QString, QString> m_clipboardProperties;

  // Transform controls for aspect ratio lock
  bool m_lockAspectRatio = false;
  QPointF m_lastScale{1.0, 1.0};

  // PERF-2: Dirty flag system to prevent redundant updates
  // Stores property values that need to be flushed to widgets
  QHash<QString, QString> m_pendingUpdates;
  // Timer for batching updates
  class QTimer *m_updateTimer = nullptr;
  // Last known values to detect actual changes
  QHash<QString, QString> m_lastKnownValues;
  // Flush pending updates to widgets
  void flushPendingUpdates();
};

} // namespace NovelMind::editor::qt
