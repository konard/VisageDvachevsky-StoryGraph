#pragma once

/**
 * @file nm_script_editor_panel.hpp
 * @brief Script editor panel for NMScript editing with IDE-like features
 *
 * Enhanced IDE features include:
 * - Go-to Definition (Ctrl+Click / F12)
 * - Find References (Shift+F12)
 * - Symbol Navigator (Ctrl+Shift+O)
 * - Code Snippets (scene, choice, if, goto templates)
 * - Inline error markers with underlines
 * - Script-to-Graph navigation
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_issues_panel.hpp"
#include <QHash>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QSplitter>
#include <QSyntaxHighlighter>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>

class QCompleter;
class QFileSystemWatcher;
class NMIssuesPanel;

namespace NovelMind::editor::qt {

/**
 * @brief Syntax highlighter for NMScript with error/warning underlines
 */
class NMScriptHighlighter final : public QSyntaxHighlighter {
  Q_OBJECT

public:
  explicit NMScriptHighlighter(QTextDocument *parent = nullptr);

  /**
   * @brief Set diagnostic markers for inline error/warning highlighting
   * @param diagnostics Map of line number (1-based) to list of issues
   */
  void setDiagnostics(const QHash<int, QList<NMScriptIssue>> &diagnostics);

  /**
   * @brief Clear all diagnostic markers
   */
  void clearDiagnostics();

protected:
  void highlightBlock(const QString &text) override;

private:
  struct Rule {
    QRegularExpression pattern;
    QTextCharFormat format;
  };

  QVector<Rule> m_rules;
  QTextCharFormat m_commentFormat;
  QTextCharFormat m_errorFormat;
  QTextCharFormat m_warningFormat;
  QRegularExpression m_commentStart;
  QRegularExpression m_commentEnd;
  QHash<int, QList<NMScriptIssue>> m_diagnostics;
};

/**
 * @brief Location reference for symbol definitions and usages
 */
struct SymbolLocation {
  QString filePath;
  int line = 0;
  int column = 0;
  QString context; // Surrounding code line for preview
};

/**
 * @brief Enhanced NMScript editor with full IDE features
 */
class NMScriptEditor final : public QPlainTextEdit {
  Q_OBJECT

public:
  struct CompletionEntry {
    QString text;
    QString detail;
  };

  explicit NMScriptEditor(QWidget *parent = nullptr);

  void setCompletionWords(const QStringList &words);
  void setCompletionEntries(const QList<CompletionEntry> &entries);
  void setHoverDocs(const QHash<QString, QString> &docs);
  void setDocHtml(const QHash<QString, QString> &docs);
  void setProjectDocs(const QHash<QString, QString> &docs);
  [[nodiscard]] int indentSize() const { return m_indentSize; }

  /**
   * @brief Set symbol locations for go-to-definition feature
   * @param locations Map of lowercase symbol name to its definition location
   */
  void setSymbolLocations(const QHash<QString, SymbolLocation> &locations);

  /**
   * @brief Set the highlighter for diagnostic updates
   */
  void setHighlighter(NMScriptHighlighter *highlighter) {
    m_highlighter = highlighter;
  }

  /**
   * @brief Update inline diagnostics (error/warning underlines)
   */
  void setDiagnostics(const QList<NMScriptIssue> &issues);

  int lineNumberAreaWidth() const;
  void lineNumberAreaPaintEvent(QPaintEvent *event);

  /**
   * @brief Insert a code snippet at cursor position
   * @param snippetType Type of snippet: "scene", "choice", "if", "goto"
   */
  void insertSnippet(const QString &snippetType);

signals:
  void requestSave();
  void hoverDocChanged(const QString &token, const QString &html);

  /**
   * @brief Emitted when go-to-definition is triggered (F12 or Ctrl+Click)
   */
  void goToDefinitionRequested(const QString &symbol,
                               const SymbolLocation &location);

  /**
   * @brief Emitted when find-references is triggered (Shift+F12)
   */
  void findReferencesRequested(const QString &symbol);

  /**
   * @brief Emitted to request navigation to a graph node
   */
  void navigateToGraphNodeRequested(const QString &sceneId);

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  QString textUnderCursor() const;
  QString symbolAtPosition(const QPoint &pos) const;
  void insertCompletion(const QString &completion);
  void refreshDynamicCompletions();
  void rebuildCompleterModel(const QList<CompletionEntry> &entries);
  void updateLineNumberAreaWidth(int newBlockCount = 0);
  void updateLineNumberArea(const QRect &rect, int dy);
  void highlightCurrentLine();
  void handleTabKey(QKeyEvent *event);
  void handleBacktabKey(QKeyEvent *event);
  void handleReturnKey(QKeyEvent *event);
  void indentSelection(int delta);
  QString indentForCurrentLine(int *outLogicalIndent = nullptr) const;

  void goToDefinition();
  void findReferences();
  void showSnippetMenu();

  QCompleter *m_completer = nullptr;
  NMScriptHighlighter *m_highlighter = nullptr;
  QHash<QString, QString> m_hoverDocs;
  QHash<QString, QString> m_docHtml;
  QHash<QString, QString> m_projectDocs;
  QHash<QString, SymbolLocation> m_symbolLocations;
  QStringList m_baseCompletionWords;
  QString m_lastHoverToken;
  QList<CompletionEntry> m_staticCompletionEntries;
  QList<CompletionEntry> m_cachedCompletionEntries;
  QWidget *m_lineNumberArea = nullptr;
  int m_indentSize = 2;
};

/**
 * @brief Reference result for Find References feature
 */
struct ReferenceResult {
  QString filePath;
  int line = 0;
  QString context;
  bool isDefinition = false;
};

/**
 * @brief Enhanced Script Editor Panel with full IDE features
 *
 * Features:
 * - Go-to Definition (F12 / Ctrl+Click)
 * - Find References (Shift+F12)
 * - Symbol Navigator (Ctrl+Shift+O)
 * - Code Snippets with template expansion
 * - Inline error/warning markers
 * - Integration with Story Graph
 */
class NMScriptEditorPanel final : public NMDockPanel {
  Q_OBJECT

public:
  explicit NMScriptEditorPanel(QWidget *parent = nullptr);
  void setIssuesPanel(class NMIssuesPanel *panel) { m_issuesPanel = panel; }
  ~NMScriptEditorPanel() override;

  void onInitialize() override;
  void onUpdate(double deltaTime) override;

  void openScript(const QString &path);
  void refreshFileList();
  void goToLocation(const QString &path, int line);

  /**
   * @brief Navigate to a scene definition
   * @param sceneName The scene name to navigate to
   * @return true if navigation succeeded
   */
  bool goToSceneDefinition(const QString &sceneName);

  /**
   * @brief Find all references to a symbol across all scripts
   * @param symbol The symbol to find references for
   * @return List of reference locations
   */
  QList<ReferenceResult> findAllReferences(const QString &symbol) const;

  /**
   * @brief Get the symbol index for external use
   */
  [[nodiscard]] const auto &symbolIndex() const { return m_symbolIndex; }

signals:
  void docHtmlChanged(const QString &html);

  /**
   * @brief Emitted when user requests to navigate to a graph node from script
   */
  void navigateToGraphNode(const QString &sceneId);

  /**
   * @brief Emitted when find references results are ready
   */
  void referencesFound(const QString &symbol,
                       const QList<ReferenceResult> &references);

private slots:
  void onFileActivated(QTreeWidgetItem *item, int column);
  void onSaveRequested();
  void onSaveAllRequested();
  void onFormatRequested();
  void onCurrentTabChanged(int index);
  void onSymbolListActivated(QListWidgetItem *item);
  void onSymbolNavigatorRequested();
  void onGoToDefinition(const QString &symbol, const SymbolLocation &location);
  void onFindReferences(const QString &symbol);
  void onInsertSnippetRequested();
  void onNavigateToGraphNode(const QString &sceneId);
  void runDiagnostics();

private:
  void setupContent();
  void setupToolBar();
  void addEditorTab(const QString &path);
  bool saveEditor(QPlainTextEdit *editor);
  bool ensureScriptFile(const QString &path);
  QList<NMScriptIssue> validateSource(const QString &path,
                                      const QString &source) const;
  void refreshSymbolIndex();
  void pushCompletionsToEditors();
  void refreshSymbolList();
  void filterSymbolList(const QString &filter);
  QList<NMScriptEditor::CompletionEntry> buildProjectCompletionEntries() const;
  QHash<QString, QString> buildProjectHoverDocs() const;
  QHash<QString, QString> buildProjectDocHtml() const;
  QHash<QString, SymbolLocation> buildSymbolLocations() const;
  void rebuildWatchList();
  QString scriptsRootPath() const;
  QList<NMScriptEditor *> editors() const;
  void showReferencesDialog(const QString &symbol,
                            const QList<ReferenceResult> &references);
  NMScriptEditor *currentEditor() const;

  QWidget *m_contentWidget = nullptr;
  QSplitter *m_splitter = nullptr;
  QSplitter *m_leftSplitter = nullptr;
  QTreeWidget *m_fileTree = nullptr;
  QListWidget *m_symbolList = nullptr;
  QTabWidget *m_tabs = nullptr;
  QToolBar *m_toolBar = nullptr;

  QHash<QWidget *, QString> m_tabPaths;
  QPointer<QFileSystemWatcher> m_scriptWatcher;

  struct ScriptSymbolIndex {
    QHash<QString, QString> scenes;     // name -> file path
    QHash<QString, QString> characters; // name -> file path
    QHash<QString, QString> flags;      // name -> file path
    QHash<QString, QString> variables;  // name -> file path
    QStringList backgrounds;            // asset ids seen in scripts
    QStringList voices;                 // voice ids seen in scripts
    QStringList music;                  // music ids seen in scripts

    // Extended location info for go-to-definition
    QHash<QString, int> sceneLines;     // name -> line number
    QHash<QString, int> characterLines; // name -> line number
  } m_symbolIndex;

  QTimer m_diagnosticsTimer;
  class NMIssuesPanel *m_issuesPanel = nullptr;
};

} // namespace NovelMind::editor::qt
