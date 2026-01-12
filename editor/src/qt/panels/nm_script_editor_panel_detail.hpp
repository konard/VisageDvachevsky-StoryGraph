#pragma once

#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"

namespace NovelMind::editor::qt::detail {

QStringList buildCompletionWords();
QHash<QString, QString> buildHoverDocs();
QHash<QString, QString> buildDocHtml();
QList<NMScriptEditor::CompletionEntry> buildKeywordEntries();

/**
 * @brief Build snippet templates for code insertion
 * @return List of snippet templates with tabstop placeholders
 */
QList<SnippetTemplate> buildSnippetTemplates();

/**
 * @brief Get context-specific completion suggestions
 * @param context The current completion context
 * @param symbolIndex Symbol index for project symbols
 * @return List of completion entries appropriate for the context
 */
QList<NMScriptEditor::CompletionEntry>
getContextCompletions(CompletionContext context, const QHash<QString, QString>& scenes,
                      const QHash<QString, QString>& characters,
                      const QHash<QString, QString>& flags,
                      const QHash<QString, QString>& variables, const QStringList& backgrounds,
                      const QStringList& music, const QStringList& voices);

/**
 * @brief Analyze diagnostics and generate quick fixes
 * @param issues List of script issues
 * @param source The source code
 * @return Map of line number to quick fixes
 */
QHash<int, QList<QuickFix>> generateQuickFixes(const QList<NMScriptIssue>& issues,
                                               const QString& source);

/**
 * @brief Get syntax hint for a keyword at cursor
 * @param keyword The keyword under or before cursor
 * @return A short syntax hint string
 */
QString getSyntaxHintForKeyword(const QString& keyword);

} // namespace NovelMind::editor::qt::detail
