/**
 * @file tutorial_manager.cpp
 * @brief Implementation of the Tutorial Manager
 */

#include "NovelMind/editor/guided_learning/tutorial_manager.hpp"
#include "NovelMind/editor/event_bus.hpp"
#include "NovelMind/editor/guided_learning/anchor_registry.hpp"
#include "NovelMind/editor/guided_learning/help_overlay.hpp"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace NovelMind::editor::guided_learning {

namespace {

std::string getCurrentTimestamp() {
  auto now = std::chrono::system_clock::now();
  auto time = std::chrono::system_clock::to_time_t(now);
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time), "%Y-%m-%dT%H:%M:%S");
  return ss.str();
}

TutorialStep parseStepFromJson(const QJsonObject& obj) {
  TutorialStep step;
  step.id = obj["id"].toString().toStdString();
  step.title = obj["title"].toString().toStdString();
  step.content = obj["content"].toString().toStdString();
  step.anchorId = obj["anchorId"].toString().toStdString();

  if (obj.contains("hintType")) {
    auto ht = parseHintType(obj["hintType"].toString().toStdString());
    if (ht)
      step.hintType = *ht;
  }

  if (obj.contains("position")) {
    auto pos = parseCalloutPosition(obj["position"].toString().toStdString());
    if (pos)
      step.position = *pos;
  }

  step.showBackButton = obj.value("showBackButton").toBool(true);
  step.showSkipButton = obj.value("showSkipButton").toBool(true);
  step.showDontShowAgain = obj.value("showDontShowAgain").toBool(true);

  step.autoHide = obj.value("autoHide").toBool(false);
  step.autoHideDelaySeconds = static_cast<f32>(obj.value("autoHideDelaySeconds").toDouble(5.0));

  // Parse advance condition
  if (obj.contains("advanceCondition")) {
    QJsonObject condObj = obj["advanceCondition"].toObject();
    QString condType = condObj["type"].toString();

    if (condType == "UserAcknowledge") {
      step.advanceCondition.type = StepCondition::Type::UserAcknowledge;
    } else if (condType == "ElementClick") {
      step.advanceCondition.type = StepCondition::Type::ElementClick;
      step.advanceCondition.targetAnchorId = condObj["targetAnchorId"].toString().toStdString();
    } else if (condType == "ElementFocus") {
      step.advanceCondition.type = StepCondition::Type::ElementFocus;
      step.advanceCondition.targetAnchorId = condObj["targetAnchorId"].toString().toStdString();
    } else if (condType == "PanelOpened") {
      step.advanceCondition.type = StepCondition::Type::PanelOpened;
      step.advanceCondition.targetAnchorId = condObj["panelId"].toString().toStdString();
    } else if (condType == "Timeout") {
      step.advanceCondition.type = StepCondition::Type::Timeout;
      step.advanceCondition.timeoutSeconds =
          static_cast<f32>(condObj["timeoutSeconds"].toDouble(5.0));
    }
  }

  return step;
}

TutorialDefinition parseTutorialFromJson(const QJsonObject& obj) {
  TutorialDefinition tutorial;
  tutorial.id = obj["id"].toString().toStdString();
  tutorial.title = obj["title"].toString().toStdString();
  tutorial.description = obj["description"].toString().toStdString();
  tutorial.category = obj["category"].toString().toStdString();

  if (obj.contains("level")) {
    auto lvl = parseTutorialLevel(obj["level"].toString().toStdString());
    if (lvl)
      tutorial.level = *lvl;
  }

  if (obj.contains("trigger")) {
    auto tr = parseTutorialTrigger(obj["trigger"].toString().toStdString());
    if (tr)
      tutorial.trigger = *tr;
  }

  tutorial.triggerPanelId = obj.value("triggerPanelId").toString().toStdString();
  tutorial.featureVersion = obj.value("featureVersion").toString().toStdString();

  // Parse steps
  QJsonArray stepsArray = obj["steps"].toArray();
  for (const auto& stepVal : stepsArray) {
    tutorial.steps.push_back(parseStepFromJson(stepVal.toObject()));
  }

  // Parse prerequisites
  QJsonArray prereqArray = obj["prerequisites"].toArray();
  for (const auto& prereq : prereqArray) {
    tutorial.prerequisiteTutorials.push_back(prereq.toString().toStdString());
  }

  // Parse tags
  QJsonArray tagsArray = obj["tags"].toArray();
  for (const auto& tag : tagsArray) {
    tutorial.tags.push_back(tag.toString().toStdString());
  }

  tutorial.author = obj.value("author").toString().toStdString();
  tutorial.lastUpdated = obj.value("lastUpdated").toString().toStdString();
  tutorial.estimatedMinutes = static_cast<u32>(obj.value("estimatedMinutes").toInt(5));

  return tutorial;
}

ContextualHint parseHintFromJson(const QJsonObject& obj) {
  ContextualHint hint;
  hint.id = obj["id"].toString().toStdString();
  hint.content = obj["content"].toString().toStdString();
  hint.anchorId = obj["anchorId"].toString().toStdString();

  if (obj.contains("hintType")) {
    auto ht = parseHintType(obj["hintType"].toString().toStdString());
    if (ht)
      hint.hintType = *ht;
  }

  if (obj.contains("position")) {
    auto pos = parseCalloutPosition(obj["position"].toString().toStdString());
    if (pos)
      hint.position = *pos;
  }

  hint.triggerCondition = obj.value("triggerCondition").toString().toStdString();
  hint.maxShowCount = static_cast<u32>(obj.value("maxShowCount").toInt(3));
  hint.showOncePerSession = obj.value("showOncePerSession").toBool(false);
  hint.autoHide = obj.value("autoHide").toBool(true);
  hint.autoHideDelaySeconds = static_cast<f32>(obj.value("autoHideDelaySeconds").toDouble(8.0));

  return hint;
}

} // anonymous namespace

// ============================================================================
// NMTutorialManager Implementation
// ============================================================================

NMTutorialManager& NMTutorialManager::instance() {
  static NMTutorialManager instance;
  return instance;
}

NMTutorialManager::NMTutorialManager() : QObject(nullptr) {
  setObjectName("NMTutorialManager");
}

void NMTutorialManager::initialize(NMHelpOverlay* overlay) {
  if (m_initialized) {
    qWarning() << "NMTutorialManager already initialized";
    return;
  }

  m_overlay = overlay;

  // Connect overlay signals
  if (m_overlay) {
    connect(m_overlay, &NMHelpOverlay::nextClicked, this, &NMTutorialManager::nextStep);
    connect(m_overlay, &NMHelpOverlay::backClicked, this, &NMTutorialManager::previousStep);
    connect(m_overlay, &NMHelpOverlay::skipClicked, this, &NMTutorialManager::skipStep);
    connect(m_overlay, &NMHelpOverlay::closeClicked, this, [this]() { stopTutorial(false); });
    connect(m_overlay, &NMHelpOverlay::dontShowAgainToggled, this, [this](bool checked) {
      if (m_activeTutorial && checked) {
        disableTutorial(m_activeTutorial->id);
      }
    });
  }

  connectToEventBus();
  m_initialized = true;

  qDebug() << "NMTutorialManager initialized";
}

void NMTutorialManager::shutdown() {
  if (!m_initialized)
    return;

  stopTutorial(false);
  hideAllHints();

  if (m_overlay) {
    disconnect(m_overlay, nullptr, this, nullptr);
  }

  m_overlay = nullptr;
  m_initialized = false;

  qDebug() << "NMTutorialManager shutdown";
}

Result<u32> NMTutorialManager::loadTutorialsFromDirectory(const std::string& directory) {
  QDir dir(QString::fromStdString(directory));
  if (!dir.exists()) {
    return Result<u32>::error("Directory does not exist: " + directory);
  }

  QStringList filters;
  filters << "*.json";
  QFileInfoList files = dir.entryInfoList(filters, QDir::Files);

  u32 loadedCount = 0;
  for (const auto& fileInfo : files) {
    auto result = loadTutorialFromFile(fileInfo.absoluteFilePath().toStdString());
    if (result.isOk()) {
      loadedCount++;
    } else {
      qWarning() << "Failed to load tutorial:" << fileInfo.fileName() << "-"
                 << QString::fromStdString(result.error());
    }
  }

  qDebug() << "Loaded" << loadedCount << "tutorials from" << QString::fromStdString(directory);
  return Result<u32>::ok(loadedCount);
}

Result<void> NMTutorialManager::loadTutorialFromFile(const std::string& filePath) {
  QFile file(QString::fromStdString(filePath));
  if (!file.open(QIODevice::ReadOnly)) {
    return Result<void>::error("Cannot open file: " + filePath);
  }

  QByteArray data = file.readAll();
  return loadTutorialFromJson(QString::fromUtf8(data).toStdString());
}

Result<void> NMTutorialManager::loadTutorialFromJson(const std::string& jsonContent) {
  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(jsonContent), &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    return Result<void>::error("JSON parse error: " + parseError.errorString().toStdString());
  }

  QJsonObject root = doc.object();

  // Check if this is a tutorial or hints file
  if (root.contains("tutorial")) {
    TutorialDefinition tutorial = parseTutorialFromJson(root["tutorial"].toObject());
    registerTutorial(tutorial);
  } else if (root.contains("tutorials")) {
    QJsonArray tutorialsArray = root["tutorials"].toArray();
    for (const auto& tutorialVal : tutorialsArray) {
      TutorialDefinition tutorial = parseTutorialFromJson(tutorialVal.toObject());
      registerTutorial(tutorial);
    }
  }

  if (root.contains("hints")) {
    QJsonArray hintsArray = root["hints"].toArray();
    for (const auto& hintVal : hintsArray) {
      ContextualHint hint = parseHintFromJson(hintVal.toObject());
      registerHint(hint);
    }
  }

  return Result<void>::ok();
}

void NMTutorialManager::registerTutorial(const TutorialDefinition& tutorial) {
  m_tutorials[tutorial.id] = tutorial;
  qDebug() << "Registered tutorial:" << QString::fromStdString(tutorial.id);
}

void NMTutorialManager::unregisterTutorial(const std::string& tutorialId) {
  m_tutorials.erase(tutorialId);
}

std::optional<TutorialDefinition>
NMTutorialManager::getTutorial(const std::string& tutorialId) const {
  auto it = m_tutorials.find(tutorialId);
  if (it != m_tutorials.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<TutorialDefinition> NMTutorialManager::getAllTutorials() const {
  std::vector<TutorialDefinition> result;
  result.reserve(m_tutorials.size());
  for (const auto& [_, tutorial] : m_tutorials) {
    result.push_back(tutorial);
  }
  return result;
}

std::vector<TutorialDefinition>
NMTutorialManager::getTutorialsByCategory(const std::string& category) const {
  std::vector<TutorialDefinition> result;
  for (const auto& [_, tutorial] : m_tutorials) {
    if (tutorial.category == category) {
      result.push_back(tutorial);
    }
  }
  return result;
}

std::vector<TutorialDefinition> NMTutorialManager::getTutorialsByLevel(TutorialLevel level) const {
  std::vector<TutorialDefinition> result;
  for (const auto& [_, tutorial] : m_tutorials) {
    if (tutorial.level == level) {
      result.push_back(tutorial);
    }
  }
  return result;
}

std::vector<TutorialDefinition> NMTutorialManager::searchTutorials(const std::string& query) const {
  std::vector<TutorialDefinition> result;
  QString lowerQuery = QString::fromStdString(query).toLower();

  for (const auto& [_, tutorial] : m_tutorials) {
    bool matches = false;

    // Check title
    if (QString::fromStdString(tutorial.title).toLower().contains(lowerQuery)) {
      matches = true;
    }
    // Check description
    if (QString::fromStdString(tutorial.description).toLower().contains(lowerQuery)) {
      matches = true;
    }
    // Check tags
    for (const auto& tag : tutorial.tags) {
      if (QString::fromStdString(tag).toLower().contains(lowerQuery)) {
        matches = true;
        break;
      }
    }

    if (matches) {
      result.push_back(tutorial);
    }
  }

  return result;
}

void NMTutorialManager::registerHint(const ContextualHint& hint) {
  m_hints[hint.id] = hint;
}

void NMTutorialManager::unregisterHint(const std::string& hintId) {
  m_hints.erase(hintId);
}

std::optional<ContextualHint> NMTutorialManager::getHint(const std::string& hintId) const {
  auto it = m_hints.find(hintId);
  if (it != m_hints.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::vector<ContextualHint> NMTutorialManager::getAllHints() const {
  std::vector<ContextualHint> result;
  result.reserve(m_hints.size());
  for (const auto& [_, hint] : m_hints) {
    result.push_back(hint);
  }
  return result;
}

bool NMTutorialManager::startTutorial(const std::string& tutorialId) {
  if (!m_initialized || !m_overlay) {
    qWarning() << "NMTutorialManager not initialized";
    return false;
  }

  if (m_progress.globallyDisabled) {
    qDebug() << "Guided learning is globally disabled";
    return false;
  }

  auto it = m_tutorials.find(tutorialId);
  if (it == m_tutorials.end()) {
    qWarning() << "Tutorial not found:" << QString::fromStdString(tutorialId);
    return false;
  }

  // Check if disabled
  if (isTutorialDisabled(tutorialId)) {
    qDebug() << "Tutorial is disabled:" << QString::fromStdString(tutorialId);
    return false;
  }

  // Check prerequisites
  if (!arePrerequisitesMet(tutorialId)) {
    qDebug() << "Prerequisites not met for:" << QString::fromStdString(tutorialId);
    return false;
  }

  // Stop any currently active tutorial
  if (m_activeTutorial) {
    stopTutorial(false);
  }

  m_activeTutorial = it->second;
  m_currentStepIndex = 0;

  // Initialize progress if needed
  auto& progress = m_progress.tutorials[tutorialId];
  if (progress.state == TutorialState::NotStarted) {
    progress.tutorialId = tutorialId;
    progress.state = TutorialState::InProgress;
    progress.startedAt = getCurrentTimestamp();
    progress.stepStates.resize(m_activeTutorial->steps.size(), StepState::Pending);
  }

  displayCurrentStep();

  emit tutorialStarted(QString::fromStdString(tutorialId));
  emit progressChanged();

  return true;
}

void NMTutorialManager::stopTutorial(bool markComplete) {
  if (!m_activeTutorial)
    return;

  std::string tutorialId = m_activeTutorial->id;

  hideCurrentStep();

  if (markComplete) {
    auto& progress = m_progress.tutorials[tutorialId];
    progress.state = TutorialState::Completed;
    progress.completedAt = getCurrentTimestamp();
    emit tutorialCompleted(QString::fromStdString(tutorialId));
  } else {
    emit tutorialStopped(QString::fromStdString(tutorialId));
  }

  m_activeTutorial.reset();
  m_currentStepIndex = 0;

  emit progressChanged();
}

void NMTutorialManager::nextStep() {
  if (!m_activeTutorial || !m_overlay)
    return;

  // Mark current step as completed
  updateStepProgress();

  // Move to next step
  m_currentStepIndex++;

  if (m_currentStepIndex >= m_activeTutorial->steps.size()) {
    // Tutorial complete
    stopTutorial(true);
  } else {
    displayCurrentStep();
    emit tutorialStepChanged(QString::fromStdString(m_activeTutorial->id),
                             static_cast<int>(m_currentStepIndex));
  }

  emit progressChanged();
}

void NMTutorialManager::previousStep() {
  if (!m_activeTutorial || m_currentStepIndex == 0)
    return;

  m_currentStepIndex--;
  displayCurrentStep();

  emit tutorialStepChanged(QString::fromStdString(m_activeTutorial->id),
                           static_cast<int>(m_currentStepIndex));
}

void NMTutorialManager::skipStep() {
  if (!m_activeTutorial)
    return;

  // Mark current step as skipped
  auto& progress = m_progress.tutorials[m_activeTutorial->id];
  if (m_currentStepIndex < progress.stepStates.size()) {
    progress.stepStates[m_currentStepIndex] = StepState::Skipped;
  }

  nextStep();
}

void NMTutorialManager::skipAll() {
  if (!m_activeTutorial)
    return;

  // Mark all remaining steps as skipped
  auto& progress = m_progress.tutorials[m_activeTutorial->id];
  for (size_t i = m_currentStepIndex; i < progress.stepStates.size(); i++) {
    if (progress.stepStates[i] == StepState::Pending) {
      progress.stepStates[i] = StepState::Skipped;
    }
  }

  stopTutorial(true);
}

std::optional<std::string> NMTutorialManager::getActiveTutorialId() const {
  if (m_activeTutorial) {
    return m_activeTutorial->id;
  }
  return std::nullopt;
}

std::optional<TutorialStep> NMTutorialManager::getCurrentStep() const {
  if (!m_activeTutorial || m_currentStepIndex >= m_activeTutorial->steps.size()) {
    return std::nullopt;
  }
  return m_activeTutorial->steps[m_currentStepIndex];
}

bool NMTutorialManager::showHint(const std::string& hintId) {
  if (!m_initialized || !m_overlay) {
    return false;
  }

  if (!m_progress.hintsEnabled) {
    return false;
  }

  auto it = m_hints.find(hintId);
  if (it == m_hints.end()) {
    return false;
  }

  const auto& hint = it->second;

  // Check if disabled
  auto& hintProgress = m_progress.hints[hintId];
  if (hintProgress.disabled) {
    return false;
  }

  // Check max show count
  if (hintProgress.showCount >= hint.maxShowCount) {
    return false;
  }

  // Show the hint
  m_overlay->showHint(hintId, hint.content, hint.anchorId, hint.hintType, hint.position,
                      hint.autoHide, static_cast<int>(hint.autoHideDelaySeconds * 1000));

  // Update progress
  hintProgress.hintId = hintId;
  hintProgress.showCount++;
  hintProgress.lastShownAt = getCurrentTimestamp();

  m_visibleHints.insert(hintId);

  emit hintShown(QString::fromStdString(hintId));
  return true;
}

void NMTutorialManager::hideHint(const std::string& hintId) {
  if (m_overlay) {
    m_overlay->hideHint(hintId);
  }
  m_visibleHints.erase(hintId);
  emit hintHidden(QString::fromStdString(hintId));
}

void NMTutorialManager::hideAllHints() {
  if (m_overlay) {
    m_overlay->hideAll();
  }

  for (const auto& hintId : m_visibleHints) {
    emit hintHidden(QString::fromStdString(hintId));
  }
  m_visibleHints.clear();
}

bool NMTutorialManager::isHintVisible(const std::string& hintId) const {
  return m_visibleHints.count(hintId) > 0;
}

TutorialProgress NMTutorialManager::getTutorialProgress(const std::string& tutorialId) const {
  auto it = m_progress.tutorials.find(tutorialId);
  if (it != m_progress.tutorials.end()) {
    return it->second;
  }
  return TutorialProgress{tutorialId, TutorialState::NotStarted, 0, {}, "", "", false, false};
}

HintProgress NMTutorialManager::getHintProgress(const std::string& hintId) const {
  auto it = m_progress.hints.find(hintId);
  if (it != m_progress.hints.end()) {
    return it->second;
  }
  return HintProgress{hintId, 0, false, ""};
}

void NMTutorialManager::resetTutorialProgress(const std::string& tutorialId) {
  m_progress.tutorials.erase(tutorialId);
  emit progressChanged();
}

void NMTutorialManager::resetHintProgress(const std::string& hintId) {
  m_progress.hints.erase(hintId);
  emit progressChanged();
}

void NMTutorialManager::resetAllProgress() {
  m_progress.tutorials.clear();
  m_progress.hints.clear();
  m_progress.seenFeatureVersions.clear();
  emit progressChanged();
}

void NMTutorialManager::disableTutorial(const std::string& tutorialId) {
  m_progress.tutorials[tutorialId].disabled = true;
  m_progress.tutorials[tutorialId].state = TutorialState::Disabled;
  emit progressChanged();
}

void NMTutorialManager::enableTutorial(const std::string& tutorialId) {
  auto& progress = m_progress.tutorials[tutorialId];
  progress.disabled = false;
  if (progress.state == TutorialState::Disabled) {
    progress.state = TutorialState::NotStarted;
  }
  emit progressChanged();
}

void NMTutorialManager::disableHint(const std::string& hintId) {
  m_progress.hints[hintId].disabled = true;
  emit progressChanged();
}

void NMTutorialManager::enableHint(const std::string& hintId) {
  m_progress.hints[hintId].disabled = false;
  emit progressChanged();
}

bool NMTutorialManager::isTutorialCompleted(const std::string& tutorialId) const {
  auto it = m_progress.tutorials.find(tutorialId);
  if (it != m_progress.tutorials.end()) {
    return it->second.state == TutorialState::Completed;
  }
  return false;
}

bool NMTutorialManager::isTutorialDisabled(const std::string& tutorialId) const {
  auto it = m_progress.tutorials.find(tutorialId);
  if (it != m_progress.tutorials.end()) {
    return it->second.disabled || it->second.state == TutorialState::Disabled;
  }
  return false;
}

bool NMTutorialManager::arePrerequisitesMet(const std::string& tutorialId) const {
  auto it = m_tutorials.find(tutorialId);
  if (it == m_tutorials.end()) {
    return false;
  }

  for (const auto& prereqId : it->second.prerequisiteTutorials) {
    if (!isTutorialCompleted(prereqId)) {
      return false;
    }
  }
  return true;
}

Result<void> NMTutorialManager::loadProgress(const std::string& filePath) {
  QFile file(QString::fromStdString(filePath));
  if (!file.open(QIODevice::ReadOnly)) {
    return Result<void>::error("Cannot open progress file: " + filePath);
  }

  QJsonParseError parseError;
  QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError) {
    return Result<void>::error("JSON parse error: " + parseError.errorString().toStdString());
  }

  QJsonObject root = doc.object();

  m_progress.globallyDisabled = root.value("globallyDisabled").toBool(false);
  m_progress.hintsEnabled = root.value("hintsEnabled").toBool(true);
  m_progress.walkthroughsOnFirstRun = root.value("walkthroughsOnFirstRun").toBool(true);

  // Load tutorial progress
  QJsonObject tutorialsObj = root["tutorials"].toObject();
  for (auto it = tutorialsObj.begin(); it != tutorialsObj.end(); ++it) {
    QJsonObject progObj = it.value().toObject();
    TutorialProgress prog;
    prog.tutorialId = it.key().toStdString();
    prog.state = static_cast<TutorialState>(progObj["state"].toInt());
    prog.currentStepIndex = static_cast<u32>(progObj["currentStepIndex"].toInt());
    prog.disabled = progObj["disabled"].toBool();
    prog.neverShowAgain = progObj["neverShowAgain"].toBool();
    prog.startedAt = progObj["startedAt"].toString().toStdString();
    prog.completedAt = progObj["completedAt"].toString().toStdString();

    QJsonArray statesArray = progObj["stepStates"].toArray();
    for (const auto& stateVal : statesArray) {
      prog.stepStates.push_back(static_cast<StepState>(stateVal.toInt()));
    }

    m_progress.tutorials[prog.tutorialId] = prog;
  }

  // Load hint progress
  QJsonObject hintsObj = root["hints"].toObject();
  for (auto it = hintsObj.begin(); it != hintsObj.end(); ++it) {
    QJsonObject progObj = it.value().toObject();
    HintProgress prog;
    prog.hintId = it.key().toStdString();
    prog.showCount = static_cast<u32>(progObj["showCount"].toInt());
    prog.disabled = progObj["disabled"].toBool();
    prog.lastShownAt = progObj["lastShownAt"].toString().toStdString();

    m_progress.hints[prog.hintId] = prog;
  }

  // Load seen feature versions
  QJsonObject versionsObj = root["seenFeatureVersions"].toObject();
  for (auto it = versionsObj.begin(); it != versionsObj.end(); ++it) {
    m_progress.seenFeatureVersions[it.key().toStdString()] = it.value().toString().toStdString();
  }

  emit progressChanged();
  emit settingsChanged();

  return Result<void>::ok();
}

Result<void> NMTutorialManager::saveProgress(const std::string& filePath) const {
  QJsonObject root;

  root["globallyDisabled"] = m_progress.globallyDisabled;
  root["hintsEnabled"] = m_progress.hintsEnabled;
  root["walkthroughsOnFirstRun"] = m_progress.walkthroughsOnFirstRun;

  // Save tutorial progress
  QJsonObject tutorialsObj;
  for (const auto& [id, prog] : m_progress.tutorials) {
    QJsonObject progObj;
    progObj["state"] = static_cast<int>(prog.state);
    progObj["currentStepIndex"] = static_cast<int>(prog.currentStepIndex);
    progObj["disabled"] = prog.disabled;
    progObj["neverShowAgain"] = prog.neverShowAgain;
    progObj["startedAt"] = QString::fromStdString(prog.startedAt);
    progObj["completedAt"] = QString::fromStdString(prog.completedAt);

    QJsonArray statesArray;
    for (auto state : prog.stepStates) {
      statesArray.append(static_cast<int>(state));
    }
    progObj["stepStates"] = statesArray;

    tutorialsObj[QString::fromStdString(id)] = progObj;
  }
  root["tutorials"] = tutorialsObj;

  // Save hint progress
  QJsonObject hintsObj;
  for (const auto& [id, prog] : m_progress.hints) {
    QJsonObject progObj;
    progObj["showCount"] = static_cast<int>(prog.showCount);
    progObj["disabled"] = prog.disabled;
    progObj["lastShownAt"] = QString::fromStdString(prog.lastShownAt);

    hintsObj[QString::fromStdString(id)] = progObj;
  }
  root["hints"] = hintsObj;

  // Save seen feature versions
  QJsonObject versionsObj;
  for (const auto& [featureId, version] : m_progress.seenFeatureVersions) {
    versionsObj[QString::fromStdString(featureId)] = QString::fromStdString(version);
  }
  root["seenFeatureVersions"] = versionsObj;

  QFile file(QString::fromStdString(filePath));
  if (!file.open(QIODevice::WriteOnly)) {
    return Result<void>::error("Cannot open file for writing: " + filePath);
  }

  QJsonDocument doc(root);
  file.write(doc.toJson(QJsonDocument::Indented));

  return Result<void>::ok();
}

void NMTutorialManager::setProgressFilePath(const std::string& filePath) {
  m_progressFilePath = filePath;
}

void NMTutorialManager::autoSaveProgress() {
  if (!m_progressFilePath.empty()) {
    saveProgress(m_progressFilePath);
  }
}

void NMTutorialManager::setEnabled(bool enabled) {
  m_progress.globallyDisabled = !enabled;
  if (!enabled && m_activeTutorial) {
    stopTutorial(false);
  }
  if (!enabled) {
    hideAllHints();
  }
  emit settingsChanged();
}

void NMTutorialManager::setHintsEnabled(bool enabled) {
  m_progress.hintsEnabled = enabled;
  if (!enabled) {
    hideAllHints();
  }
  emit settingsChanged();
}

void NMTutorialManager::setWalkthroughsOnFirstRunEnabled(bool enabled) {
  m_progress.walkthroughsOnFirstRun = enabled;
  emit settingsChanged();
}

void NMTutorialManager::registerCustomCondition(const std::string& conditionId,
                                                CustomConditionCallback callback) {
  m_customConditions[conditionId] = std::move(callback);
}

void NMTutorialManager::unregisterCustomCondition(const std::string& conditionId) {
  m_customConditions.erase(conditionId);
}

void NMTutorialManager::onPanelOpened(const std::string& panelId) {
  if (!m_progress.walkthroughsOnFirstRun || m_progress.globallyDisabled) {
    return;
  }

  // Find tutorials triggered by this panel
  for (const auto& [id, tutorial] : m_tutorials) {
    if (tutorial.trigger == TutorialTrigger::FirstRun && tutorial.triggerPanelId == panelId) {
      // Check if already seen
      if (!isTutorialCompleted(id) && !isTutorialDisabled(id)) {
        startTutorial(id);
        break; // Only start one tutorial at a time
      }
    }
  }
}

void NMTutorialManager::onPanelEmptyState(const std::string& panelId, bool isEmpty) {
  if (!m_progress.hintsEnabled || m_progress.globallyDisabled) {
    return;
  }

  // Find hints triggered by empty state
  for (const auto& [id, hint] : m_hints) {
    if (hint.triggerCondition == "panel.empty" && hint.anchorId.find(panelId) == 0) {
      if (isEmpty) {
        showHint(id);
      } else {
        hideHint(id);
      }
    }
  }
}

void NMTutorialManager::onErrorOccurred(const std::string& errorCode, const std::string& context) {
  if (m_progress.globallyDisabled) {
    return;
  }

  // Find tutorials triggered by this error
  for (const auto& [id, tutorial] : m_tutorials) {
    if (tutorial.trigger == TutorialTrigger::OnError) {
      // Could add more sophisticated error matching here
      if (!isTutorialCompleted(id) && !isTutorialDisabled(id)) {
        startTutorial(id);
        break;
      }
    }
  }

  (void)errorCode;
  (void)context;
}

void NMTutorialManager::onFeatureVersionEncountered(const std::string& featureId,
                                                    const std::string& version) {
  if (!m_progress.walkthroughsOnFirstRun || m_progress.globallyDisabled) {
    return;
  }

  // Check if this version was already seen
  auto it = m_progress.seenFeatureVersions.find(featureId);
  if (it != m_progress.seenFeatureVersions.end() && it->second == version) {
    return; // Already seen this version
  }

  // Mark as seen
  m_progress.seenFeatureVersions[featureId] = version;

  // Find tutorials triggered by this feature version
  for (const auto& [id, tutorial] : m_tutorials) {
    if (tutorial.trigger == TutorialTrigger::FirstRun && tutorial.featureVersion == version) {
      if (!isTutorialCompleted(id) && !isTutorialDisabled(id)) {
        startTutorial(id);
        break;
      }
    }
  }
}

void NMTutorialManager::displayCurrentStep() {
  if (!m_activeTutorial || !m_overlay)
    return;

  if (m_currentStepIndex >= m_activeTutorial->steps.size())
    return;

  const auto& step = m_activeTutorial->steps[m_currentStepIndex];

  m_overlay->showTutorialStep(
      step.id, step.title, step.content, step.anchorId, step.hintType, step.position,
      static_cast<int>(m_currentStepIndex), static_cast<int>(m_activeTutorial->steps.size()),
      step.showBackButton && m_currentStepIndex > 0, step.showSkipButton, step.showDontShowAgain);

  // Update progress
  auto& progress = m_progress.tutorials[m_activeTutorial->id];
  if (m_currentStepIndex < progress.stepStates.size()) {
    progress.stepStates[m_currentStepIndex] = StepState::Active;
  }
  progress.currentStepIndex = m_currentStepIndex;
}

void NMTutorialManager::hideCurrentStep() {
  if (m_activeTutorial && m_overlay) {
    const auto& step = m_activeTutorial->steps[m_currentStepIndex];
    m_overlay->hideHint(step.id);
  }
}

void NMTutorialManager::updateStepProgress() {
  if (!m_activeTutorial)
    return;

  auto& progress = m_progress.tutorials[m_activeTutorial->id];
  if (m_currentStepIndex < progress.stepStates.size()) {
    progress.stepStates[m_currentStepIndex] = StepState::Completed;
  }
}

bool NMTutorialManager::evaluateCondition(const StepCondition& condition) {
  switch (condition.type) {
  case StepCondition::Type::UserAcknowledge:
    return true; // Always satisfied when user clicks Next

  case StepCondition::Type::ElementClick:
  case StepCondition::Type::ElementFocus:
    // These are handled via events
    return false;

  case StepCondition::Type::Custom:
    if (!condition.customConditionId.empty()) {
      auto it = m_customConditions.find(condition.customConditionId);
      if (it != m_customConditions.end()) {
        return it->second();
      }
    }
    return false;

  default:
    return true;
  }
}

void NMTutorialManager::connectToEventBus() {
  // Subscribe to relevant events
  auto& bus = EventBus::instance();

  // Panel focus changes
  bus.subscribe<PanelFocusChangedEvent>([this](const PanelFocusChangedEvent& event) {
    if (event.hasFocus) {
      onPanelOpened(event.panelName);
    }
  });

  // Error events
  bus.subscribe<ErrorEvent>(
      [this](const ErrorEvent& event) { onErrorOccurred(event.message, event.details); });
}

} // namespace NovelMind::editor::guided_learning
