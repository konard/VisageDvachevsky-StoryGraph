#pragma once

#include <QColor>
#include <QDialog>
#include <QLineEdit>
#include <QList>
#include <QString>
#include <QStringList>

class QComboBox;
class QListWidget;
class QDoubleSpinBox;
class QFrame;
class QListView;
class QLabel;
class QPushButton;
class QSpinBox;
class QTextEdit;
class QTreeView;
class QFileSystemModel;
class QSortFilterProxyModel;

namespace NovelMind::editor::qt {

enum class NMDialogButton { None, Ok, Cancel, Yes, No, Save, Discard, Close };

enum class NMMessageType { Info, Warning, Error, Question };

class NMMessageDialog final : public QDialog {
  Q_OBJECT

public:
  NMMessageDialog(QWidget* parent, const QString& title, const QString& message, NMMessageType type,
                  const QList<NMDialogButton>& buttons,
                  NMDialogButton defaultButton = NMDialogButton::Ok);

  [[nodiscard]] NMDialogButton choice() const { return m_choice; }

  static NMDialogButton showInfo(QWidget* parent, const QString& title, const QString& message);
  static NMDialogButton showWarning(QWidget* parent, const QString& title, const QString& message);
  static NMDialogButton showError(QWidget* parent, const QString& title, const QString& message);
  static NMDialogButton showQuestion(QWidget* parent, const QString& title, const QString& message,
                                     const QList<NMDialogButton>& buttons,
                                     NMDialogButton defaultButton);

private:
  void buildUi(const QString& message, NMMessageType type, const QList<NMDialogButton>& buttons,
               NMDialogButton defaultButton);

  NMDialogButton m_choice = NMDialogButton::None;
};

class NMInputDialog final : public QDialog {
  Q_OBJECT

public:
  static QString getText(QWidget* parent, const QString& title, const QString& label,
                         QLineEdit::EchoMode mode = QLineEdit::Normal,
                         const QString& text = QString(), bool* ok = nullptr);

  static int getInt(QWidget* parent, const QString& title, const QString& label, int value = 0,
                    int minValue = -2147483647, int maxValue = 2147483647, int step = 1,
                    bool* ok = nullptr);

  static double getDouble(QWidget* parent, const QString& title, const QString& label,
                          double value = 0.0, double minValue = -1.0e308, double maxValue = 1.0e308,
                          int decimals = 2, bool* ok = nullptr);

  static QString getItem(QWidget* parent, const QString& title, const QString& label,
                         const QStringList& items, int current = 0, bool editable = false,
                         bool* ok = nullptr);

  static QString getMultiLineText(QWidget* parent, const QString& title, const QString& label,
                                  const QString& text = QString(), bool* ok = nullptr);

private:
  enum class InputType { Text, Int, Double, Item, MultiLine };

  explicit NMInputDialog(QWidget* parent, const QString& title, const QString& label,
                         InputType type);

  void configureText(const QString& text, QLineEdit::EchoMode mode);
  void configureInt(int value, int minValue, int maxValue, int step);
  void configureDouble(double value, double minValue, double maxValue, int decimals);
  void configureItem(const QStringList& items, int current, bool editable);
  void configureMultiLine(const QString& text);

  QString textValue() const;
  int intValue() const;
  double doubleValue() const;
  QString itemValue() const;
  QString multiLineValue() const;

  QLabel* m_label = nullptr;
  QLineEdit* m_textEdit = nullptr;
  QSpinBox* m_intSpin = nullptr;
  QDoubleSpinBox* m_doubleSpin = nullptr;
  QComboBox* m_comboBox = nullptr;
  ::QTextEdit* m_multiLineEdit = nullptr;
  QPushButton* m_okButton = nullptr;
  QPushButton* m_cancelButton = nullptr;
  InputType m_type = InputType::Text;
};

class NMFileDialog final : public QDialog {
  Q_OBJECT

public:
  enum class Mode { OpenFile, OpenFiles, SaveFile, SelectDirectory };

  static QString getOpenFileName(QWidget* parent, const QString& title,
                                 const QString& dir = QString(), const QString& filter = QString());
  static QStringList getOpenFileNames(QWidget* parent, const QString& title,
                                      const QString& dir = QString(),
                                      const QString& filter = QString());
  static QString getSaveFileName(QWidget* parent, const QString& title,
                                 const QString& dir = QString(), const QString& filter = QString());
  static QString getExistingDirectory(QWidget* parent, const QString& title,
                                      const QString& dir = QString());

private:
  explicit NMFileDialog(QWidget* parent, const QString& title, Mode mode, const QString& dir,
                        const QString& filter);

  void buildUi();
  void applyFilter(const QString& filterText);
  void setDirectory(const QString& path);
  void updateAcceptState();
  void updatePreview();
  void acceptSelection();
  QStringList selectedFilePaths() const;
  QString selectedDirectoryPath() const;

  Mode m_mode = Mode::OpenFile;
  QString m_currentDir;
  QStringList m_selectedPaths;

  QTreeView* m_treeView = nullptr;
  QListView* m_listView = nullptr;
  QLineEdit* m_pathEdit = nullptr;
  QComboBox* m_filterCombo = nullptr;
  QLabel* m_selectionLabel = nullptr;
  QPushButton* m_upButton = nullptr;
  QPushButton* m_acceptButton = nullptr;
  QPushButton* m_cancelButton = nullptr;
  QLabel* m_previewImage = nullptr;
  QLabel* m_previewName = nullptr;
  QLabel* m_previewMeta = nullptr;
  QLineEdit* m_filenameEdit = nullptr;

  ::QFileSystemModel* m_dirModel = nullptr;
  ::QFileSystemModel* m_fileModel = nullptr;
  ::QSortFilterProxyModel* m_filterProxy = nullptr;
};

class NMColorDialog final : public QDialog {
  Q_OBJECT

public:
  static QColor getColor(const QColor& initial, QWidget* parent = nullptr,
                         const QString& title = QString(), bool* ok = nullptr);

private:
  explicit NMColorDialog(QWidget* parent, const QColor& initial, const QString& title);

  void setColor(const QColor& color, bool updateFields = true);
  QColor currentColor() const;
  void syncFromHex();
  void updatePreview();

  QFrame* m_preview = nullptr;
  QSpinBox* m_redSpin = nullptr;
  QSpinBox* m_greenSpin = nullptr;
  QSpinBox* m_blueSpin = nullptr;
  QLineEdit* m_hexEdit = nullptr;
  QPushButton* m_okButton = nullptr;
  QPushButton* m_cancelButton = nullptr;
};

class NMNewProjectDialog final : public QDialog {
  Q_OBJECT

public:
  explicit NMNewProjectDialog(QWidget* parent = nullptr);

  void setTemplateOptions(const QStringList& templates);
  void setTemplate(const QString& templateName);
  void setProjectName(const QString& name);
  void setBaseDirectory(const QString& directory);
  void setResolution(const QString& resolution);
  void setLocale(const QString& locale);
  void setWorkflowMode(int modeIndex);

  [[nodiscard]] QString projectName() const;
  [[nodiscard]] QString baseDirectory() const;
  [[nodiscard]] QString projectPath() const;
  [[nodiscard]] QString templateName() const;
  [[nodiscard]] QString resolution() const;
  [[nodiscard]] QString locale() const;
  [[nodiscard]] int workflowMode() const;

  // Common resolutions for visual novels
  static QStringList standardResolutions();
  // Common locales
  static QStringList standardLocales();
  // Workflow modes (issue #100)
  static QStringList workflowModes();

private:
  void buildUi();
  void updatePreview();
  void updateCreateEnabled();
  void updateWorkflowDescription();
  void browseDirectory();

  QLineEdit* m_nameEdit = nullptr;
  QLineEdit* m_directoryEdit = nullptr;
  QComboBox* m_templateCombo = nullptr;
  QComboBox* m_resolutionCombo = nullptr;
  QComboBox* m_localeCombo = nullptr;
  QComboBox* m_workflowCombo = nullptr;
  QLabel* m_workflowDescription = nullptr;
  QLabel* m_pathPreview = nullptr;
  QPushButton* m_browseButton = nullptr;
  QPushButton* m_createButton = nullptr;
  QPushButton* m_cancelButton = nullptr;
};

/**
 * @brief Dialog for editing voice line metadata (tags, notes, speaker, scene)
 *
 * Provides a comprehensive interface for editing voice line metadata
 * including tags, notes, speaker assignment, and scene information.
 */
class NMVoiceMetadataDialog final : public QDialog {
  Q_OBJECT

public:
  struct MetadataResult {
    QStringList tags;
    QString notes;
    QString speaker;
    QString scene;
  };

  explicit NMVoiceMetadataDialog(QWidget* parent, const QString& lineId,
                                 const QStringList& currentTags, const QString& currentNotes,
                                 const QString& currentSpeaker, const QString& currentScene,
                                 const QStringList& availableSpeakers = {},
                                 const QStringList& availableScenes = {},
                                 const QStringList& suggestedTags = {});

  [[nodiscard]] MetadataResult result() const { return m_result; }

  /**
   * @brief Static convenience method to show the dialog and get results
   * @return true if user accepted, false if cancelled
   */
  static bool getMetadata(QWidget* parent, const QString& lineId, const QStringList& currentTags,
                          const QString& currentNotes, const QString& currentSpeaker,
                          const QString& currentScene, MetadataResult& outResult,
                          const QStringList& availableSpeakers = {},
                          const QStringList& availableScenes = {},
                          const QStringList& suggestedTags = {});

private:
  void buildUi(const QString& lineId, const QStringList& currentTags, const QString& currentNotes,
               const QString& currentSpeaker, const QString& currentScene,
               const QStringList& availableSpeakers, const QStringList& availableScenes,
               const QStringList& suggestedTags);
  void onAddTag();
  void onRemoveTag();
  void onTagSuggestionClicked(const QString& tag);
  void updateResult();

  QLineEdit* m_tagInput = nullptr;
  QListWidget* m_tagList = nullptr;
  ::QTextEdit* m_notesEdit = nullptr;
  QComboBox* m_speakerCombo = nullptr;
  QComboBox* m_sceneCombo = nullptr;
  QPushButton* m_addTagBtn = nullptr;
  QPushButton* m_removeTagBtn = nullptr;
  QPushButton* m_okButton = nullptr;
  QPushButton* m_cancelButton = nullptr;
  QWidget* m_suggestionsWidget = nullptr;

  MetadataResult m_result;
};

} // namespace NovelMind::editor::qt

// Forward declarations for NMNewSceneDialog
namespace NovelMind::editor {
class SceneTemplateManager;
struct SceneTemplateMetadata;
} // namespace NovelMind::editor

namespace NovelMind::editor::qt {

/**
 * @brief Dialog for creating a new scene from a template
 *
 * Provides template selection with preview thumbnails, scene name input,
 * and "Start from blank" option. Implements issue #216.
 */
class NMNewSceneDialog final : public QDialog {
  Q_OBJECT

public:
  explicit NMNewSceneDialog(QWidget* parent,
                            ::NovelMind::editor::SceneTemplateManager* templateManager);

  /**
   * @brief Get the selected template ID
   * @return Template ID or empty string if "Start from blank" selected
   */
  [[nodiscard]] QString selectedTemplateId() const { return m_selectedTemplateId; }

  /**
   * @brief Get the entered scene name
   * @return Scene name
   */
  [[nodiscard]] QString sceneName() const;

  /**
   * @brief Get the generated scene ID (sanitized from name)
   * @return Scene ID
   */
  [[nodiscard]] QString sceneId() const;

  /**
   * @brief Check if "Start from blank" was selected
   * @return true if blank scene
   */
  [[nodiscard]] bool isBlankScene() const { return m_selectedTemplateId.isEmpty(); }

  /**
   * @brief Static convenience method to show dialog and get result
   * @param parent Parent widget
   * @param templateManager Template manager instance
   * @param outSceneId Output: Scene ID
   * @param outTemplateId Output: Template ID (empty for blank)
   * @return true if user accepted, false if cancelled
   */
  static bool getNewScene(QWidget* parent,
                          ::NovelMind::editor::SceneTemplateManager* templateManager,
                          QString& outSceneId, QString& outTemplateId);

private:
  void buildUi();
  void populateTemplateList();
  void onTemplateSelected();
  void onCategoryChanged(int index);
  void updatePreview();
  void updateCreateEnabled();

  ::NovelMind::editor::SceneTemplateManager* m_templateManager = nullptr;
  QString m_selectedTemplateId;

  QLineEdit* m_nameEdit = nullptr;
  QComboBox* m_categoryCombo = nullptr;
  QListWidget* m_templateList = nullptr;
  QLabel* m_previewImage = nullptr;
  QLabel* m_previewName = nullptr;
  QLabel* m_previewDescription = nullptr;
  QLabel* m_sceneIdPreview = nullptr;
  QPushButton* m_createButton = nullptr;
  QPushButton* m_cancelButton = nullptr;
};

} // namespace NovelMind::editor::qt
