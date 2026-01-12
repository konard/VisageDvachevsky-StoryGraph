#include "NovelMind/editor/qt/panels/nm_script_editor_panel.hpp"
#include "NovelMind/editor/qt/widgets/nm_scene_preview_widget.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_icon_manager.hpp"
#include "NovelMind/editor/qt/nm_play_mode_controller.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_issues_panel.hpp"
#include "NovelMind/editor/qt/dialogs/nm_script_welcome_dialog.hpp"
#include "NovelMind/editor/script_project_context.hpp"
#include "NovelMind/scripting/compiler.hpp"
#include "NovelMind/scripting/lexer.hpp"
#include "NovelMind/scripting/parser.hpp"
#include "NovelMind/scripting/validator.hpp"

#include <QAbstractItemView>
#include <QCompleter>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMutexLocker>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSet>
#include <QSettings>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStringListModel>
#include <QStyledItemDelegate>
#include <QTextBlock>
#include <QTextFormat>
#include <QTextStream>
#include <QToolButton>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QWindow>
#include <algorithm>
#include <filesystem>
#include <limits>

#include "nm_script_editor_panel_detail.hpp"

namespace NovelMind::editor::qt {

void NMScriptEditorPanel::loadSampleScript(const QString& sampleId) {
  // Sample script content based on sampleId
  QString scriptContent;
  QString fileName;

  if (sampleId == "basic") {
    fileName = "sample_basic.nms";
    scriptContent = R"(// Sample Script: Basic Scene
// A simple introduction to NMScript with dialogue and characters

// Define characters
character Alice(name="Alice", color="#4A90D9")
character Bob(name="Bob", color="#E74C3C")

// Main scene
scene intro {
    // Set the background
    show background "bg_room"

    // Show Alice at center
    show Alice at center

    // Basic dialogue
    say Alice "Hello! Welcome to the Script Editor."
    say Alice "This is a basic scene demonstrating character dialogue."

    // Show Bob entering
    show Bob at right with slide_left

    say Bob "Hi Alice! Great to be here."
    say Alice "Let me show you around!"

    // Transition to next scene
    goto exploration
}

scene exploration {
    say Alice "You can create scenes, characters, and dialogue easily."
    say Bob "This is amazing!"

    // End of sample
    say Alice "Try experimenting with your own scripts!"
}
)";
  } else if (sampleId == "choices") {
    fileName = "sample_choices.nms";
    scriptContent = R"(// Sample Script: Choice System
// Demonstrates branching dialogue with player choices and flags

character Hero(name="Hero", color="#2ECC71")
character Guide(name="Guide", color="#3498DB")

scene start {
    show background "bg_crossroads"
    show Guide at center

    say Guide "Welcome, traveler! You've reached a crossroads."
    say Guide "Which path will you choose?"

    // Player choice
    choice {
        "Take the forest path" -> {
            set flag chose_forest = true
            say Hero "I'll go through the forest."
            goto forest_path
        }
        "Take the mountain pass" -> {
            set flag chose_mountain = true
            say Hero "The mountain pass looks challenging."
            goto mountain_path
        }
        "Ask for advice" -> {
            say Hero "What would you recommend?"
            say Guide "Both paths have their rewards."
            goto start
        }
    }
}

scene forest_path {
    show background "bg_forest"
    say Hero "The forest is beautiful and peaceful."

    if flag chose_forest {
        say Guide "A wise choice for those who appreciate nature."
    }

    goto ending
}

scene mountain_path {
    show background "bg_mountain"
    say Hero "The view from up here is breathtaking!"

    if flag chose_mountain {
        say Guide "The brave path rewards those who take it."
    }

    goto ending
}

scene ending {
    say Guide "Your journey continues..."
    say Hero "Thank you for the guidance!"
}
)";
  } else if (sampleId == "advanced") {
    fileName = "sample_advanced.nms";
    scriptContent = R"(// Sample Script: Advanced Features
// Showcases variables, conditionals, transitions, and more

character Sage(name="Elder Sage", color="#9B59B6")
character Player(name="You", color="#1ABC9C")

scene intro {
    // Fade transition
    transition fade 1.0

    show background "bg_temple"
    play music "ambient_mystical" loop=true

    wait 0.5

    show Sage at center with fade

    say Sage "Welcome to the ancient temple."
    say Sage "Let me test your wisdom..."

    // Initialize score variable
    set score = 0
    set max_questions = 3

    goto question_1
}

scene question_1 {
    say Sage "First question: What is the most valuable treasure?"

    choice {
        "Gold and jewels" -> {
            say Player "Wealth and riches!"
            set score = score + 0
            goto question_2
        }
        "Knowledge and wisdom" -> {
            say Player "Knowledge that lasts forever."
            set score = score + 10
            say Sage "A wise answer."
            goto question_2
        }
        "Friends and family" -> {
            say Player "The people we love."
            set score = score + 10
            say Sage "True wisdom."
            goto question_2
        }
    }
}

scene question_2 {
    say Sage "Second question: How do you face challenges?"

    choice {
        "With courage" -> {
            set score = score + 10
            goto question_3
        }
        "With caution" -> {
            set score = score + 5
            goto question_3
        }
        "By avoiding them" -> {
            set score = score + 0
            goto question_3
        }
    }
}

scene question_3 {
    say Sage "Final question: What drives you forward?"

    choice {
        "Personal glory" -> goto results
        "Helping others" -> {
            set score = score + 10
            goto results
        }
        "Curiosity" -> {
            set score = score + 5
            goto results
        }
    }
}

scene results {
    say Sage "Let me see your results..."

    wait 1.0

    if score >= 25 {
        // High score path
        play sound "success_chime"
        say Sage "Exceptional! You possess great wisdom."
        show Sage with "proud"
        goto good_ending
    } else if score >= 15 {
        // Medium score
        say Sage "Good! You show promise."
        goto normal_ending
    } else {
        // Low score
        say Sage "You have much to learn, young one."
        goto learning_ending
    }
}

scene good_ending {
    transition fade 0.5
    show background "bg_temple_golden"

    say Sage "I shall teach you the ancient arts."

    flash color="#FFD700" duration=0.3

    say Player "Thank you, Master!"

    transition fade 2.0
}

scene normal_ending {
    say Sage "Return when you've gained more experience."
    say Player "I will!"
}

scene learning_ending {
    say Sage "Study and return to try again."
    say Player "I understand."
}
)";
  } else {
    core::Logger::instance().warning("Unknown sample script ID: " + sampleId.toStdString());
    return;
  }

  // Create sample script in project scripts folder
  const QString scriptsPath = scriptsRootPath();
  if (scriptsPath.isEmpty()) {
    core::Logger::instance().warning("Cannot load sample script: No scripts folder found");
    return;
  }

  const QString fullPath = scriptsPath + "/" + fileName;

  // Write the sample script
  QFile file(fullPath);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream out(&file);
    out << scriptContent;
    file.close();

    core::Logger::instance().info("Created sample script: " + fullPath.toStdString());

    // Refresh file list and open the sample
    refreshFileList();
    openScript(fullPath);
  } else {
    core::Logger::instance().error("Failed to create sample script: " + fullPath.toStdString());
  }
}

void NMScriptEditorPanel::saveState() {
  QSettings settings;

  // Save splitter positions
  if (m_splitter) {
    settings.setValue("scriptEditor/splitterState", m_splitter->saveState());
  }
  if (m_leftSplitter) {
    settings.setValue("scriptEditor/leftSplitterState", m_leftSplitter->saveState());
  }

  // Save open files
  QStringList openFiles;
  for (int i = 0; i < m_tabs->count(); ++i) {
    QWidget* widget = m_tabs->widget(i);
    QString path = m_tabPaths.value(widget);
    if (!path.isEmpty()) {
      openFiles.append(path);
    }
  }
  settings.setValue("scriptEditor/openFiles", openFiles);

  // Save active file index
  settings.setValue("scriptEditor/activeFileIndex", m_tabs->currentIndex());

  // Save cursor positions for each open file
  for (int i = 0; i < m_tabs->count(); ++i) {
    if (auto* editor = qobject_cast<NMScriptEditor*>(m_tabs->widget(i))) {
      QString path = m_tabPaths.value(editor);
      if (!path.isEmpty()) {
        QTextCursor cursor = editor->textCursor();
        settings.setValue(QString("scriptEditor/cursorPos/%1").arg(path), cursor.position());
      }
    }
  }

  // Save minimap visibility
  settings.setValue("scriptEditor/minimapVisible", m_minimapEnabled);
}

void NMScriptEditorPanel::restoreState() {
  QSettings settings;

  // Restore splitter positions
  if (m_splitter) {
    QByteArray state = settings.value("scriptEditor/splitterState").toByteArray();
    if (!state.isEmpty()) {
      m_splitter->restoreState(state);
    }
  }
  if (m_leftSplitter) {
    QByteArray state = settings.value("scriptEditor/leftSplitterState").toByteArray();
    if (!state.isEmpty()) {
      m_leftSplitter->restoreState(state);
    }
  }

  // Restore minimap visibility
  m_minimapEnabled = settings.value("scriptEditor/minimapVisible", true).toBool();

  // Restore open files if enabled
  bool restoreOpenFiles = settings.value("editor.script.restore_open_files", true).toBool();
  if (restoreOpenFiles) {
    QStringList openFiles = settings.value("scriptEditor/openFiles").toStringList();
    int activeIndex = settings.value("scriptEditor/activeFileIndex", 0).toInt();

    for (const QString& path : openFiles) {
      if (QFileInfo::exists(path)) {
        openScript(path);

        // Restore cursor position if enabled
        bool restoreCursor = settings.value("editor.script.restore_cursor_position", true).toBool();
        if (restoreCursor) {
          if (auto* editor = qobject_cast<NMScriptEditor*>(m_tabs->currentWidget())) {
            int cursorPos =
                settings.value(QString("scriptEditor/cursorPos/%1").arg(path), 0).toInt();
            QTextCursor cursor = editor->textCursor();
            cursor.setPosition(cursorPos);
            editor->setTextCursor(cursor);
          }
        }
      }
    }

    // Restore active tab
    if (activeIndex >= 0 && activeIndex < m_tabs->count()) {
      m_tabs->setCurrentIndex(activeIndex);
    }
  }

  // Apply settings from registry
  applySettings();
}

void NMScriptEditorPanel::applySettings() {
  QSettings settings;

  // Apply diagnostic delay
  int diagnosticDelay = settings.value("editor.script.diagnostic_delay", 600).toInt();
  m_diagnosticsTimer.setInterval(diagnosticDelay);

  // Apply settings to all open editors
  for (auto* editor : editors()) {
    if (!editor)
      continue;

    // Font settings
    QString fontFamily = settings.value("editor.script.font_family", "monospace").toString();
    int fontSize = settings.value("editor.script.font_size", 14).toInt();
    QFont font(fontFamily, fontSize);
    editor->setFont(font);

    // Display settings
    bool showMinimap = settings.value("editor.script.show_minimap", true).toBool();
    editor->setMinimapEnabled(showMinimap);
    m_minimapEnabled = showMinimap;

    // Word wrap
    bool wordWrap = settings.value("editor.script.word_wrap", false).toBool();
    editor->setLineWrapMode(wordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);

    // Tab size (note: the editor already has a tab size property)
    int tabSize = settings.value("editor.script.tab_size", 4).toInt();
    QFontMetrics metrics(font);
    editor->setTabStopDistance(tabSize * metrics.horizontalAdvance(' '));
  }
}

} // namespace NovelMind::editor::qt
