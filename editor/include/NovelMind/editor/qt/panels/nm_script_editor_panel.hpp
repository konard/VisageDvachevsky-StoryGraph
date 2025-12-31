#pragma once

/**
 * @file nm_script_editor_panel.hpp
 * @brief Script editor panel for NMScript editing with full IDE features
 *
 * Full-featured IDE for NMScript with professional editing capabilities:
 *
 * Core IDE Features:
 * - Context-aware autocompletion with smart suggestions
 * - Real-time error/warning highlighting with detailed tooltips
 * - Go-to Definition (Ctrl+Click / F12)
 * - Find References (Shift+F12)
 * - Symbol Navigator/Outline (Ctrl+Shift+O)
 * - Code Snippets with tabstop placeholders (scene, choice, if, goto, etc.)
 * - Inline quick help and documentation popups
 * - Script-to-Graph bidirectional navigation
 *
 * Editor Features:
 * - Minimap (code overview on right side)
 * - Code Folding (collapse/expand blocks)
 * - Bracket Matching (highlight matching brackets)
 * - Find and Replace with regex (Ctrl+F / Ctrl+H)
 * - Command Palette (Ctrl+Shift+P)
 * - Auto-formatting and linting
 * - Quick fixes for common errors
 * - Status bar with syntax hints
 * - Breadcrumb navigation
 */

#include "NovelMind/editor/qt/nm_dock_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_issues_panel.hpp"
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSplitter>
#include <QSyntaxHighlighter>
#include <QTabWidget>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <functional>

class QCompleter;
class QFileSystemWatcher;
class QCheckBox;
class QPushButton;
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
 * @brief Completion context for context-aware suggestions
 */
enum class CompletionContext {
  Unknown,         // General completion
  AfterScene,      // After "scene" keyword - suggest scene names
  AfterCharacter,  // After "character" keyword
  AfterSay,        // After "say" - suggest character names
  AfterShow,       // After "show" - suggest background/character
  AfterHide,       // After "hide" - suggest visible elements
  AfterGoto,       // After "goto" - suggest scene names
  AfterPlay,       // After "play" - suggest music/sound/voice
  AfterStop,       // After "stop" - suggest channels
  AfterSet,        // After "set" - suggest variables/flags
  AfterIf,         // After "if" - suggest conditions
  AfterChoice,     // Inside choice block
  AfterAt,         // After "at" - suggest positions
  AfterTransition, // After "transition" - suggest transition types
  InString,        // Inside a string literal
  InComment        // Inside a comment
};

/**
 * @brief Quick fix action for diagnostics
 */
struct QuickFix {
  QString title;
  QString description;
  int line = 0;
  int column = 0;
  QString replacement;
  int replacementLength = 0; // Length of text to replace (0 for insert)
};

/**
 * @brief Snippet with tabstop placeholders for smart insertion
 */
struct SnippetTemplate {
  QString name;
  QString prefix; // Trigger text
  QString description;
  QString body;         // Snippet body with ${1:placeholder} syntax
  QStringList tabstops; // Extracted tabstop values
};

// Forward declarations for new VSCode-like features
class NMScriptEditor;
class NMScriptMinimap;
class NMFindReplaceWidget;
class NMScriptCommandPalette;

/**
 * @brief Minimap widget for code overview (VSCode-like)
 *
 * Displays a scaled-down view of the entire document on the right side
 * of the editor. Clicking on the minimap navigates to that location.
 */
class NMScriptMinimap final : public QWidget {
  Q_OBJECT

public:
  explicit NMScriptMinimap(NMScriptEditor *editor, QWidget *parent = nullptr);

  /**
   * @brief Update the minimap when document content changes
   */
  void updateContent();

  /**
   * @brief Set the visible viewport region
   */
  void setViewportRange(int firstLine, int lastLine);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;

private:
  NMScriptEditor *m_editor = nullptr;
  QImage m_cachedImage;
  int m_firstVisibleLine = 0;
  int m_lastVisibleLine = 0;
  int m_totalLines = 0;
  bool m_isDragging = false;
  static constexpr int kMinimapWidth = 120;
  static constexpr double kMinimapCharWidth = 1.5;
  static constexpr double kMinimapLineHeight = 3.0;
};

/**
 * @brief Bracket position for matching
 */
struct BracketPosition {
  int position = -1;
  QChar bracket;
  bool isOpening = false;
};

/**
 * @brief Find and Replace widget (VSCode-like Ctrl+F / Ctrl+H)
 */
class NMFindReplaceWidget final : public QWidget {
  Q_OBJECT

public:
  explicit NMFindReplaceWidget(QWidget *parent = nullptr);

  /**
   * @brief Set the editor to search in
   */
  void setEditor(NMScriptEditor *editor);

  /**
   * @brief Show find mode (Ctrl+F)
   */
  void showFind();

  /**
   * @brief Show find and replace mode (Ctrl+H)
   */
  void showReplace();

  /**
   * @brief Set initial search text
   */
  void setSearchText(const QString &text);

signals:
  void closeRequested();

private slots:
  void findNext();
  void findPrevious();
  void replaceNext();
  void replaceAll();
  void onSearchTextChanged(const QString &text);

private:
  void performSearch(bool forward);
  void highlightAllMatches();
  void clearHighlights();
  int countMatches() const;
  void updateMatchCount();

  NMScriptEditor *m_editor = nullptr;
  QLineEdit *m_searchEdit = nullptr;
  QLineEdit *m_replaceEdit = nullptr;
  QWidget *m_replaceRow = nullptr;
  QCheckBox *m_caseSensitive = nullptr;
  QCheckBox *m_wholeWord = nullptr;
  QCheckBox *m_useRegex = nullptr;
  QLabel *m_matchCountLabel = nullptr;
  QPushButton *m_closeBtn = nullptr;
  QList<QTextEdit::ExtraSelection> m_searchHighlights;
};

/**
 * @brief Command palette for quick access to commands (VSCode-like
 * Ctrl+Shift+P)
 */
class NMScriptCommandPalette final : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Command entry for the palette
   */
  struct Command {
    QString name;
    QString shortcut;
    QString category;
    std::function<void()> action;
  };

  explicit NMScriptCommandPalette(QWidget *parent = nullptr);

  /**
   * @brief Register a command
   */
  void addCommand(const Command &cmd);

  /**
   * @brief Show the command palette
   */
  void show();

signals:
  void commandExecuted(const QString &commandName);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onFilterChanged(const QString &filter);
  void onItemActivated(QListWidgetItem *item);

private:
  void updateCommandList(const QString &filter);

  QLineEdit *m_filterEdit = nullptr;
  QListWidget *m_commandList = nullptr;
  QList<Command> m_commands;
};

/**
 * @brief Enhanced NMScript editor with full VSCode-like IDE features
 */
class NMScriptEditor final : public QPlainTextEdit {
  Q_OBJECT

public:
  struct CompletionEntry {
    QString text;
    QString detail;
  };

  /**
   * @brief Folding region for code collapse/expand
   */
  struct FoldingRegion {
    int startLine = 0;
    int endLine = 0;
    bool isCollapsed = false;
  };

  explicit NMScriptEditor(QWidget *parent = nullptr);
  ~NMScriptEditor() override;

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
   * @brief Paint code folding indicators in the fold margin
   */
  void foldingAreaPaintEvent(QPaintEvent *event);

  /**
   * @brief Get the folding area width
   */
  int foldingAreaWidth() const;

  /**
   * @brief Get the first visible block (wrapper for protected method)
   */
  [[nodiscard]] QTextBlock getFirstVisibleBlock() const {
    return firstVisibleBlock();
  }

  /**
   * @brief Get block bounding geometry (wrapper for protected method)
   */
  [[nodiscard]] QRectF getBlockBoundingGeometry(const QTextBlock &block) const {
    return blockBoundingGeometry(block);
  }

  /**
   * @brief Get block bounding rect (wrapper for protected method)
   */
  [[nodiscard]] QRectF getBlockBoundingRect(const QTextBlock &block) const {
    return blockBoundingRect(block);
  }

  /**
   * @brief Get content offset (wrapper for protected method)
   */
  [[nodiscard]] QPointF getContentOffset() const { return contentOffset(); }

  /**
   * @brief Insert a code snippet at cursor position
   * @param snippetType Type of snippet: "scene", "choice", "if", "goto"
   */
  void insertSnippet(const QString &snippetType);

  /**
   * @brief Insert a snippet template with tabstop navigation
   * @param snippet The snippet template to insert
   */
  void insertSnippetTemplate(const SnippetTemplate &snippet);

  /**
   * @brief Get the current completion context based on cursor position
   * @return The determined context for filtering suggestions
   */
  [[nodiscard]] CompletionContext getCompletionContext() const;

  /**
   * @brief Get context-aware completion suggestions
   * @param prefix The current typing prefix
   * @return List of completion entries filtered by context
   */
  [[nodiscard]] QList<CompletionEntry>
  getContextualCompletions(const QString &prefix) const;

  /**
   * @brief Get quick fixes for the current line
   * @return List of applicable quick fixes
   */
  [[nodiscard]] QList<QuickFix> getQuickFixes(int line) const;

  /**
   * @brief Apply a quick fix
   * @param fix The quick fix to apply
   */
  void applyQuickFix(const QuickFix &fix);

  /**
   * @brief Navigate to the next tabstop in snippet mode
   */
  void nextTabstop();

  /**
   * @brief Navigate to the previous tabstop in snippet mode
   */
  void previousTabstop();

  /**
   * @brief Check if currently in snippet navigation mode
   */
  [[nodiscard]] bool isInSnippetMode() const { return m_inSnippetMode; }

  /**
   * @brief Get syntax hint for current cursor position
   * @return A short syntax hint string for the status bar
   */
  [[nodiscard]] QString getSyntaxHint() const;

  /**
   * @brief Get breadcrumb path for current position
   * @return List of breadcrumb items (e.g., ["scene main", "choice"])
   */
  [[nodiscard]] QStringList getBreadcrumbs() const;

  /**
   * @brief Toggle folding for a line
   */
  void toggleFold(int line);

  /**
   * @brief Get all folding regions
   */
  [[nodiscard]] const QList<FoldingRegion> &foldingRegions() const {
    return m_foldingRegions;
  }

  /**
   * @brief Update folding regions based on document structure
   */
  void updateFoldingRegions();

  /**
   * @brief Set minimap enabled state
   */
  void setMinimapEnabled(bool enabled);

  /**
   * @brief Check if minimap is enabled
   */
  [[nodiscard]] bool isMinimapEnabled() const { return m_minimapEnabled; }

  /**
   * @brief Get bracket matching positions for cursor position
   */
  [[nodiscard]] BracketPosition findMatchingBracket(int position) const;

  /**
   * @brief Set search highlight positions
   */
  void setSearchHighlights(const QList<QTextEdit::ExtraSelection> &highlights);

  /**
   * @brief Clear search highlights
   */
  void clearSearchHighlights();

  /**
   * @brief Get the minimap widget
   */
  [[nodiscard]] NMScriptMinimap *minimap() const { return m_minimap; }

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

  /**
   * @brief Emitted when find dialog should be shown
   */
  void showFindRequested();

  /**
   * @brief Emitted when replace dialog should be shown
   */
  void showReplaceRequested();

  /**
   * @brief Emitted when command palette should be shown
   */
  void showCommandPaletteRequested();

  /**
   * @brief Emitted when viewport scrolling changes (for minimap sync)
   */
  void viewportChanged(int firstLine, int lastLine);

  /**
   * @brief Emitted when syntax hint changes (for status bar)
   */
  void syntaxHintChanged(const QString &hint);

  /**
   * @brief Emitted when breadcrumbs change
   */
  void breadcrumbsChanged(const QStringList &breadcrumbs);

  /**
   * @brief Emitted when quick fixes are available for current position
   */
  void quickFixesAvailable(const QList<QuickFix> &fixes);

protected:
  void keyPressEvent(QKeyEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void scrollContentsBy(int dx, int dy) override;

private:
  QString textUnderCursor() const;
  QString symbolAtPosition(const QPoint &pos) const;
  void insertCompletion(const QString &completion);
  void refreshDynamicCompletions();
  void rebuildCompleterModel(const QList<CompletionEntry> &entries);
  void updateLineNumberAreaWidth(int newBlockCount = 0);
  void updateLineNumberArea(const QRect &rect, int dy);
  void highlightCurrentLine();
  void highlightMatchingBrackets();
  void handleTabKey(QKeyEvent *event);
  void handleBacktabKey(QKeyEvent *event);
  void handleReturnKey(QKeyEvent *event);
  void indentSelection(int delta);
  QString indentForCurrentLine(int *outLogicalIndent = nullptr) const;
  void updateMinimapGeometry();
  void emitViewportChanged();

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
  QWidget *m_foldingArea = nullptr;
  NMScriptMinimap *m_minimap = nullptr;
  int m_indentSize = 2;
  bool m_minimapEnabled = true;
  QList<FoldingRegion> m_foldingRegions;
  QList<QTextEdit::ExtraSelection> m_searchHighlights;
  QList<QTextEdit::ExtraSelection> m_bracketHighlights;

  // Snippet tabstop navigation
  bool m_inSnippetMode = false;
  int m_currentTabstop = 0;
  QList<QPair<int, int>> m_tabstopPositions; // start, length pairs
  QString m_lastSyntaxHint;
  QStringList m_lastBreadcrumbs;

  // Context-aware completion
  QList<CompletionEntry> m_contextualEntries;
  mutable CompletionContext m_lastContext = CompletionContext::Unknown;

  // Quick fixes for current diagnostics
  QHash<int, QList<QuickFix>> m_quickFixes; // line -> fixes
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
 * @brief Enhanced Script Editor Panel with full VSCode-like IDE features
 *
 * Features:
 * - Go-to Definition (F12 / Ctrl+Click)
 * - Find References (Shift+F12)
 * - Symbol Navigator (Ctrl+Shift+O)
 * - Code Snippets with template expansion
 * - Inline error/warning markers
 * - Integration with Story Graph
 * - Minimap (code overview)
 * - Code Folding
 * - Bracket Matching
 * - Find and Replace (Ctrl+F / Ctrl+H)
 * - Command Palette (Ctrl+Shift+P)
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

  /**
   * @brief Show the find dialog (Ctrl+F)
   */
  void showFindDialog();

  /**
   * @brief Show the find and replace dialog (Ctrl+H)
   */
  void showReplaceDialog();

  /**
   * @brief Show the command palette (Ctrl+Shift+P)
   */
  void showCommandPalette();

  /**
   * @brief Set read-only mode for workflow enforcement
   *
   * When in read-only mode (e.g., Graph Mode workflow):
   * - A banner is displayed indicating read-only state
   * - Script editing and saving are disabled
   * - The scripts can still be viewed and navigated
   *
   * @param readOnly true to enable read-only mode
   * @param reason Optional reason text for the banner (e.g., "Graph Mode")
   */
  void setReadOnly(bool readOnly, const QString &reason = QString());

  /**
   * @brief Check if panel is in read-only mode
   */
  [[nodiscard]] bool isReadOnly() const { return m_readOnly; }

  /**
   * @brief Sync script content to Story Graph
   *
   * Parses the current script and updates the corresponding Story Graph
   * nodes with dialogue, speaker, and choice information.
   */
  void syncScriptToGraph();

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

  /**
   * @brief Emitted when script content should be synced to Story Graph
   *
   * Contains parsed scene data from script files that should update
   * the corresponding Story Graph nodes.
   */
  void syncToGraphRequested(const QString &sceneName, const QString &speaker,
                            const QString &dialogueText,
                            const QStringList &choices);

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
  void onToggleMinimap();
  void onFoldAll();
  void onUnfoldAll();
  void runDiagnostics();
  void onSyntaxHintChanged(const QString &hint);
  void onBreadcrumbsChanged(const QStringList &breadcrumbs);
  void onQuickFixRequested();
  void showQuickFixMenu(const QList<QuickFix> &fixes);

private:
  void setupContent();
  void setupToolBar();
  void setupCommandPalette();
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
  NMFindReplaceWidget *m_findReplaceWidget = nullptr;
  NMScriptCommandPalette *m_commandPalette = nullptr;

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
  bool m_minimapEnabled = true;

  // Status bar and breadcrumbs
  QWidget *m_statusBar = nullptr;
  QLabel *m_syntaxHintLabel = nullptr;
  QLabel *m_cursorPosLabel = nullptr;
  QWidget *m_breadcrumbBar = nullptr;

  // Snippet templates
  QList<SnippetTemplate> m_snippetTemplates;

  // Read-only mode for workflow enforcement (issue #117)
  bool m_readOnly = false;
  QWidget *m_readOnlyBanner = nullptr;
  QLabel *m_readOnlyLabel = nullptr;
  QPushButton *m_syncToGraphBtn = nullptr;
};

} // namespace NovelMind::editor::qt
