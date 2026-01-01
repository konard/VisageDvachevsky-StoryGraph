#include <NovelMind/editor/project_manager.hpp>
#include <NovelMind/editor/qt/nm_play_mode_controller.hpp>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QVariant>
#include <algorithm>
#include <variant>

namespace NovelMind::editor::qt {

NMPlayModeController &NMPlayModeController::instance() {
  static NMPlayModeController instance;
  return instance;
}

NMPlayModeController::NMPlayModeController(QObject *parent)
    : QObject(parent), m_runtimeTimer(new QTimer(this)) {
  m_runtimeTimer->setTimerType(Qt::PreciseTimer);
  m_runtimeTimer->setInterval(0);
  connect(m_runtimeTimer, &QTimer::timeout, this,
          &NMPlayModeController::simulateStep);

  // Wire runtime callbacks
  m_runtimeHost.setOnStateChanged([this](EditorRuntimeState state) {
    // INVARIANT: Update state BEFORE emitting signals to ensure consistency
    PlayMode oldMode = m_playMode;
    switch (state) {
    case EditorRuntimeState::Running:
      m_playMode = Playing;
      m_deltaTimer.restart();
      m_runtimeTimer->start();
      break;
    case EditorRuntimeState::Paused:
    case EditorRuntimeState::Stepping:
      m_playMode = Paused;
      m_runtimeTimer->stop();
      break;
    case EditorRuntimeState::Stopped:
    case EditorRuntimeState::Unloaded:
    case EditorRuntimeState::Error:
      m_playMode = Stopped;
      m_runtimeTimer->stop();
      m_lastSnapshot = SceneSnapshot{};
      emit sceneSnapshotUpdated();
      break;
    }
    // Only emit if mode actually changed, and only after state is updated
    if (oldMode != m_playMode) {
      emit playModeChanged(m_playMode);
    }
  });

  m_runtimeHost.setOnSceneChanged([this](const std::string &sceneId) {
    m_currentNodeId = QString::fromStdString(sceneId);
    emit currentNodeChanged(m_currentNodeId);
  });

  m_runtimeHost.setOnDialogueChanged(
      [this](const std::string &speaker, const std::string &text) {
        m_currentSpeaker = QString::fromStdString(speaker);
        m_currentDialogue = QString::fromStdString(text);
        emit dialogueLineChanged(m_currentSpeaker, m_currentDialogue);
      });

  m_runtimeHost.setOnChoicesChanged(
      [this](const std::vector<std::string> &choices) {
        QStringList list;
        for (const auto &c : choices) {
          list << QString::fromStdString(c);
        }
        m_currentChoices = list;
        m_waitingForChoice = !m_currentChoices.isEmpty();
        emit choicesChanged(m_currentChoices);
      });

  m_runtimeHost.setOnVariableChanged(
      [this](const std::string &name, const scripting::Value &value) {
        auto toVariant = [](const scripting::Value &v) -> QVariant {
          if (std::holds_alternative<i32>(v)) {
            return QVariant::fromValue(std::get<i32>(v));
          }
          if (std::holds_alternative<f32>(v)) {
            return QVariant::fromValue(std::get<f32>(v));
          }
          if (std::holds_alternative<bool>(v)) {
            return QVariant::fromValue(std::get<bool>(v));
          }
          if (std::holds_alternative<std::string>(v)) {
            return QString::fromStdString(std::get<std::string>(v));
          }
          return QVariant();
        };

        m_variables[QString::fromStdString(name)] = toVariant(value);
        emit variablesChanged(m_variables);
      });

  m_runtimeHost.setOnRuntimeError([this](const std::string &err) {
    qWarning() << "[Runtime] Error:" << QString::fromStdString(err);
    m_playMode = Stopped;
    emit playModeChanged(m_playMode);
  });
}

// === Playback Control ===

void NMPlayModeController::play() {
  if (m_playMode == Playing) {
    qDebug() << "[PlayMode] Already playing, ignoring play() call";
    return;
  }

  if (m_playMode == Paused) {
    m_runtimeHost.resume();
  } else {
    if (!ensureRuntimeLoaded()) {
      qWarning() << "[PlayMode] No project loaded for runtime";
      return;
    }
    auto result = m_runtimeHost.play();
    if (result.isError()) {
      qWarning() << "[PlayMode] Failed to start runtime:"
                 << QString::fromStdString(result.error());
      return;
    }
    m_deltaTimer.restart();
  }

  m_runtimeTimer->start();
}

void NMPlayModeController::pause() {
  if (m_playMode != Playing) {
    return; // Not playing
  }

  m_runtimeTimer->stop();
  m_runtimeHost.pause();
}

void NMPlayModeController::stop() {
  if (m_playMode == Stopped) {
    return; // Already stopped
  }

  m_runtimeTimer->stop();
  m_runtimeHost.stop();
  m_currentNodeId.clear();
  m_currentDialogue.clear();
  m_currentSpeaker.clear();
  m_currentChoices.clear();
  m_waitingForChoice = false;
  m_lastSnapshot = SceneSnapshot{};
  m_variables.clear();
  m_stackFrames.clear();
  m_flags.clear();
  m_callStack.clear();
  m_playMode = Stopped;

  emit currentNodeChanged(QString()); // Clear current node
  emit dialogueLineChanged(QString(), QString());
  emit choicesChanged(m_currentChoices);
  emit variablesChanged(m_variables);
  emit stackFramesChanged(m_stackFrames);
  emit flagsChanged(m_flags);
  emit sceneSnapshotUpdated();
  emit playModeChanged(Stopped);
}

void NMPlayModeController::shutdown() {
  if (m_runtimeTimer) {
    m_runtimeTimer->stop();
  }

  m_runtimeHost.setOnStateChanged({});
  m_runtimeHost.setOnBreakpointHit({});
  m_runtimeHost.setOnSceneChanged({});
  m_runtimeHost.setOnVariableChanged({});
  m_runtimeHost.setOnRuntimeError({});
  m_runtimeHost.setOnDialogueChanged({});
  m_runtimeHost.setOnChoicesChanged({});

  if (m_runtimeLoaded) {
    m_runtimeHost.stop();
  }
  m_runtimeHost.unloadProject();
  m_runtimeLoaded = false;
  m_playMode = Stopped;
  m_currentNodeId.clear();
  m_currentDialogue.clear();
  m_currentSpeaker.clear();
  m_currentChoices.clear();
  m_waitingForChoice = false;
  m_lastSnapshot = SceneSnapshot{};
  m_variables.clear();
  m_stackFrames.clear();
  m_flags.clear();
  m_callStack.clear();

  if (m_runtimeTimer) {
    delete m_runtimeTimer;
    m_runtimeTimer = nullptr;
  }
}

bool NMPlayModeController::loadProject(const QString &projectPath,
                                       const QString &scriptsPath,
                                       const QString &assetsPath,
                                       const QString &startScene) {
  ::NovelMind::editor::ProjectDescriptor desc;
  desc.path = projectPath.toStdString();
  desc.name = QFileInfo(projectPath).fileName().toStdString();
  desc.scriptsPath = scriptsPath.toStdString();
  desc.assetsPath = assetsPath.toStdString();
  desc.startScene = startScene.toStdString();
  if (desc.scenesPath.empty()) {
    const QDir projectDir(projectPath);
    desc.scenesPath = projectDir.filePath("Scenes").toStdString();
  }

  auto result = m_runtimeHost.loadProject(desc);
  if (result.isError()) {
    qWarning() << "[PlayMode] Failed to load project for runtime:"
               << QString::fromStdString(result.error());
    m_runtimeLoaded = false;
    return false;
  }

  m_runtimeLoaded = true;
  m_lastSnapshot = m_runtimeHost.getSceneSnapshot();
  m_totalSteps =
      static_cast<int>(std::max<size_t>(1, m_runtimeHost.getScenes().size()));
  emit sceneSnapshotUpdated();
  emit projectLoaded(projectPath);
  return true;
}

bool NMPlayModeController::loadCurrentProject() {
  auto &pm = NovelMind::editor::ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    qWarning() << "[PlayMode] No open project to load";
    return false;
  }

  return loadProject(QString::fromStdString(pm.getProjectPath()),
                     QString::fromStdString(pm.getFolderPath(
                         NovelMind::editor::ProjectFolder::Scripts)),
                     QString::fromStdString(pm.getFolderPath(
                         NovelMind::editor::ProjectFolder::Assets)),
                     QString::fromStdString(pm.getStartScene()));
}

bool NMPlayModeController::ensureRuntimeLoaded() {
  auto &pm = NovelMind::editor::ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    return false;
  }

  const auto &proj = m_runtimeHost.getProject();
  const std::string projectPath = pm.getProjectPath();
  const std::string scriptsPath =
      pm.getFolderPath(NovelMind::editor::ProjectFolder::Scripts);
  const std::string assetsPath =
      pm.getFolderPath(NovelMind::editor::ProjectFolder::Assets);
  const std::string startScene = pm.getStartScene();

  const bool needsReload = !m_runtimeLoaded || proj.path != projectPath ||
                           proj.scriptsPath != scriptsPath ||
                           proj.assetsPath != assetsPath ||
                           proj.startScene != startScene;

  if (!needsReload) {
    return true;
  }

  return loadProject(
      QString::fromStdString(projectPath), QString::fromStdString(scriptsPath),
      QString::fromStdString(assetsPath), QString::fromStdString(startScene));
}

void NMPlayModeController::stepForward() {
  if (!ensureRuntimeLoaded()) {
    return;
  }

  m_runtimeHost.simulateClick();
  m_runtimeHost.stepFrame();
  m_lastSnapshot = m_runtimeHost.getSceneSnapshot();
  emit sceneSnapshotUpdated();
}

void NMPlayModeController::selectChoice(int index) {
  if (!ensureRuntimeLoaded()) {
    return;
  }
  if (!m_waitingForChoice) {
    return;
  }
  m_runtimeHost.simulateChoiceSelect(index);
  m_lastSnapshot = m_runtimeHost.getSceneSnapshot();
  emit sceneSnapshotUpdated();
}

void NMPlayModeController::advanceDialogue() {
  if (!ensureRuntimeLoaded()) {
    return;
  }
  m_runtimeHost.simulateClick();
  m_lastSnapshot = m_runtimeHost.getSceneSnapshot();
  emit sceneSnapshotUpdated();
}

bool NMPlayModeController::saveSlot(int slot) {
  if (!ensureRuntimeLoaded()) {
    return false;
  }
  auto result = m_runtimeHost.saveGame(slot);
  if (result.isError()) {
    qWarning() << "[PlayMode] Save failed:"
               << QString::fromStdString(result.error());
    return false;
  }
  return true;
}

bool NMPlayModeController::loadSlot(int slot) {
  if (!ensureRuntimeLoaded()) {
    return false;
  }

  if (m_playMode == Playing) {
    pause();
  }

  auto result = m_runtimeHost.loadGame(slot);
  if (result.isError()) {
    qWarning() << "[PlayMode] Load failed:"
               << QString::fromStdString(result.error());
    return false;
  }
  refreshRuntimeCache();
  return true;
}

bool NMPlayModeController::saveAuto() {
  if (!ensureRuntimeLoaded()) {
    return false;
  }
  auto result = m_runtimeHost.saveAuto();
  if (result.isError()) {
    qWarning() << "[PlayMode] Auto-save failed:"
               << QString::fromStdString(result.error());
    return false;
  }
  return true;
}

bool NMPlayModeController::loadAuto() {
  if (!ensureRuntimeLoaded()) {
    return false;
  }
  if (m_playMode == Playing) {
    pause();
  }
  if (!m_runtimeHost.autoSaveExists()) {
    qWarning() << "[PlayMode] No auto-save available";
    return false;
  }
  auto result = m_runtimeHost.loadAuto();
  if (result.isError()) {
    qWarning() << "[PlayMode] Auto-load failed:"
               << QString::fromStdString(result.error());
    return false;
  }
  refreshRuntimeCache();
  return true;
}

bool NMPlayModeController::hasAutoSave() const {
  return m_runtimeHost.autoSaveExists();
}

scripting::ScriptRuntime *NMPlayModeController::getScriptRuntime() {
  return m_runtimeHost.getScriptRuntime();
}

void NMPlayModeController::refreshRuntimeCache() {
  m_currentNodeId = QString::fromStdString(m_runtimeHost.getCurrentScene());
  emit currentNodeChanged(m_currentNodeId);

  m_lastSnapshot = m_runtimeHost.getSceneSnapshot();
  emit sceneSnapshotUpdated();

  auto vars = m_runtimeHost.getVariables();
  QVariantMap varMap;
  for (const auto &pair : vars) {
    const auto &name = pair.first;
    const auto &value = pair.second;
    if (std::holds_alternative<i32>(value)) {
      varMap[QString::fromStdString(name)] =
          QVariant::fromValue(std::get<i32>(value));
    } else if (std::holds_alternative<f32>(value)) {
      varMap[QString::fromStdString(name)] =
          QVariant::fromValue(std::get<f32>(value));
    } else if (std::holds_alternative<bool>(value)) {
      varMap[QString::fromStdString(name)] =
          QVariant::fromValue(std::get<bool>(value));
    } else if (std::holds_alternative<std::string>(value)) {
      varMap[QString::fromStdString(name)] =
          QString::fromStdString(std::get<std::string>(value));
    }
  }
  m_variables = varMap;
  emit variablesChanged(m_variables);

  auto flags = m_runtimeHost.getFlags();
  QVariantMap flagMap;
  for (const auto &pair : flags) {
    flagMap[QString::fromStdString(pair.first)] =
        QVariant::fromValue(pair.second);
  }
  m_flags = flagMap;
  emit flagsChanged(m_flags);

  m_currentSpeaker = QString::fromStdString(m_lastSnapshot.dialogueSpeaker);
  m_currentDialogue = QString::fromStdString(m_lastSnapshot.dialogueText);
  emit dialogueLineChanged(m_currentSpeaker, m_currentDialogue);

  m_currentChoices.clear();
  for (const auto &choice : m_lastSnapshot.choiceOptions) {
    m_currentChoices << QString::fromStdString(choice);
  }
  m_waitingForChoice = !m_currentChoices.isEmpty();
  emit choicesChanged(m_currentChoices);
}

// === Breakpoint Management ===

void NMPlayModeController::toggleBreakpoint(const QString &nodeId) {
  if (m_breakpoints.contains(nodeId)) {
    m_breakpoints.remove(nodeId);
    qDebug() << "[Breakpoint] Removed from" << nodeId;
  } else {
    m_breakpoints.insert(nodeId);
    qDebug() << "[Breakpoint] Added to" << nodeId;
  }
  emit breakpointsChanged();
}

void NMPlayModeController::setBreakpoint(const QString &nodeId, bool enabled) {
  if (enabled) {
    m_breakpoints.insert(nodeId);
  } else {
    m_breakpoints.remove(nodeId);
  }
  emit breakpointsChanged();
}

bool NMPlayModeController::hasBreakpoint(const QString &nodeId) const {
  return m_breakpoints.contains(nodeId);
}

void NMPlayModeController::clearAllBreakpoints() {
  m_breakpoints.clear();
  emit breakpointsChanged();
  qDebug() << "[Breakpoint] Cleared all breakpoints";
}

// === Variable Inspection ===

void NMPlayModeController::setVariable(const QString &name,
                                       const QVariant &value) {
  if (m_playMode != Paused) {
    qWarning() << "[PlayMode] Cannot set variable while not paused";
    return;
  }

  scripting::Value v;
  switch (value.typeId()) {
  case QMetaType::Int:
  case QMetaType::LongLong:
    v = static_cast<i32>(value.toInt());
    break;
  case QMetaType::Double:
    v = static_cast<f32>(value.toFloat());
    break;
  case QMetaType::Bool:
    v = value.toBool();
    break;
  default:
    v = value.toString().toStdString();
    break;
  }

  m_runtimeHost.setVariable(name.toStdString(), v);

  // Refresh from runtime to keep UI consistent
  const auto vars = m_runtimeHost.getVariables();
  QVariantMap varMap;
  for (const auto &[n, val] : vars) {
    QVariant qv;
    if (std::holds_alternative<i32>(val)) {
      qv = std::get<i32>(val);
    } else if (std::holds_alternative<f32>(val)) {
      qv = std::get<f32>(val);
    } else if (std::holds_alternative<bool>(val)) {
      qv = std::get<bool>(val);
    } else if (std::holds_alternative<std::string>(val)) {
      qv = QString::fromStdString(std::get<std::string>(val));
    }
    varMap[QString::fromStdString(n)] = qv;
  }
  m_variables = varMap;
  emit variablesChanged(m_variables);

  // Update flags from runtime
  const auto flags = m_runtimeHost.getFlags();
  QVariantMap flagMap;
  for (const auto &[flagName, flagValue] : flags) {
    flagMap[QString::fromStdString(flagName)] = flagValue;
  }
  m_flags = flagMap;
  emit flagsChanged(m_flags);
  qDebug() << "[Variable] Set" << name << "=" << value;
}

// === Persistence ===

void NMPlayModeController::loadBreakpoints(const QString &projectPath) {
  QSettings settings(projectPath + "/.novelmind/breakpoints.ini",
                     QSettings::IniFormat);
  settings.beginGroup("Breakpoints");

  m_breakpoints.clear();
  const QStringList keys = settings.childKeys();
  for (const QString &key : keys) {
    if (settings.value(key).toBool()) {
      m_breakpoints.insert(key);
    }
  }

  settings.endGroup();
  emit breakpointsChanged();
  qDebug() << "[Breakpoint] Loaded" << m_breakpoints.size()
           << "breakpoints from project";
}

void NMPlayModeController::saveBreakpoints(const QString &projectPath) {
  QSettings settings(projectPath + "/.novelmind/breakpoints.ini",
                     QSettings::IniFormat);
  settings.beginGroup("Breakpoints");

  settings.remove(""); // Clear all existing
  for (const QString &nodeId : m_breakpoints) {
    settings.setValue(nodeId, true);
  }

  settings.endGroup();
  qDebug() << "[Breakpoint] Saved" << m_breakpoints.size()
           << "breakpoints to project";
}

// === Mock Runtime (Phase 5.0) ===

void NMPlayModeController::simulateStep() {
  const qint64 nanos = m_deltaTimer.nsecsElapsed();
  double deltaSeconds = 0.0;
  if (nanos > 0) {
    deltaSeconds = static_cast<double>(nanos) / 1'000'000'000.0;
  } else {
    deltaSeconds = 1.0 / 60.0; // fallback
  }
  m_deltaTimer.restart();

  m_runtimeHost.update(deltaSeconds);

  // Publish latest snapshot for SceneView/Hierarchy
  m_lastSnapshot = m_runtimeHost.getSceneSnapshot();
  emit sceneSnapshotUpdated();

  // Update variables from runtime
  const auto vars = m_runtimeHost.getVariables();
  QVariantMap varMap;
  for (const auto &[name, value] : vars) {
    QVariant v;
    if (std::holds_alternative<i32>(value)) {
      v = std::get<i32>(value);
    } else if (std::holds_alternative<f32>(value)) {
      v = std::get<f32>(value);
    } else if (std::holds_alternative<bool>(value)) {
      v = std::get<bool>(value);
    } else if (std::holds_alternative<std::string>(value)) {
      v = QString::fromStdString(std::get<std::string>(value));
    }
    varMap[QString::fromStdString(name)] = v;
  }
  m_variables = varMap;
  emit variablesChanged(m_variables);

  // Update call stack
  const auto stack = m_runtimeHost.getScriptCallStack();
  QStringList stackList;
  QVariantList framesList;
  for (const auto &frame : stack.frames) {
    QString entry = QString("%1 (IP=%2)")
                        .arg(QString::fromStdString(frame.sceneName))
                        .arg(frame.instructionPointer);
    if (!frame.functionName.empty()) {
      entry.prepend(QString::fromStdString(frame.functionName) + " ");
    }
    stackList << entry;

    QVariantMap frameMap;
    frameMap["scene"] = QString::fromStdString(frame.sceneName);
    frameMap["function"] = QString::fromStdString(frame.functionName);
    frameMap["ip"] = static_cast<int>(frame.instructionPointer);
    frameMap["line"] = static_cast<int>(frame.sourceLocation.line);
    frameMap["column"] = static_cast<int>(frame.sourceLocation.column);
    frameMap["file"] = QString::fromStdString(frame.sceneName);
    framesList.push_back(frameMap);
  }
  m_callStack = stackList;
  emit callStackChanged(m_callStack);
  m_stackFrames = framesList;
  emit stackFramesChanged(m_stackFrames);

  // Dialogue/choice wait states
  m_waitingForChoice =
      m_lastSnapshot.choiceMenuVisible || !m_lastSnapshot.choiceOptions.empty();
  m_currentChoices.clear();
  for (const auto &opt : m_lastSnapshot.choiceOptions) {
    m_currentChoices << QString::fromStdString(opt);
  }

  // Track current node/scene
  if (m_currentNodeId.isEmpty() && !m_lastSnapshot.currentSceneId.empty()) {
    m_currentNodeId = QString::fromStdString(m_lastSnapshot.currentSceneId);
  }

  // Emit lightweight execution marker for debug overlay
  m_lastStepIndex++;
  if (m_totalSteps <= 0) {
    m_totalSteps =
        static_cast<int>(std::max<size_t>(1, m_runtimeHost.getScenes().size()));
  }
  if (m_currentInstruction.isEmpty() && !m_currentNodeId.isEmpty()) {
    m_currentInstruction = QString("Scene: %1").arg(m_currentNodeId);
  }
  emit executionStepChanged(m_lastStepIndex, totalSteps(),
                            m_currentInstruction);
}

void NMPlayModeController::checkBreakpoint() {
  if (m_breakpoints.contains(m_currentNodeId)) {
    qDebug() << "[Breakpoint] Hit at node:" << m_currentNodeId;
    m_runtimeTimer->stop();
    m_runtimeHost.pause();
    m_playMode = Paused;
    emit breakpointHit(m_currentNodeId);
    emit playModeChanged(Paused);
  }
}

} // namespace NovelMind::editor::qt
