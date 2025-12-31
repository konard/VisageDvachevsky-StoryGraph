#include "NovelMind/editor/qt/panels/nm_voice_manager_panel.hpp"
#include "NovelMind/editor/interfaces/IAudioPlayer.hpp"
#include "NovelMind/editor/interfaces/QtAudioPlayer.hpp"
#include "NovelMind/editor/interfaces/ServiceLocator.hpp"
#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMediaPlayer>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSlider>
#include <QSplitter>
#include <QTextStream>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <filesystem>

#ifdef QT_DEBUG
#include <QDebug>
#endif

namespace fs = std::filesystem;

namespace NovelMind::editor::qt {

NMVoiceManagerPanel::NMVoiceManagerPanel(QWidget *parent,
                                         IAudioPlayer *audioPlayer)
    : NMDockPanel("Voice Manager", parent),
      m_manifest(std::make_unique<NovelMind::audio::VoiceManifest>()) {
  // Initialize manifest with default locale
  m_manifest->setDefaultLocale("en");
  m_manifest->addLocale("en");
  m_currentLocale = "en";

  // VM-2: Set up callbacks for auto-refresh
  setupManifestCallbacks();
}

NMVoiceManagerPanel::~NMVoiceManagerPanel() {
  // Stop any ongoing playback before destruction
  stopPlayback();

  // Stop probing
  if (m_probePlayer) {
    m_probePlayer->stop();
  }
}

void NMVoiceManagerPanel::onInitialize() {
  setupUI();
  setupMediaPlayer();
}

void NMVoiceManagerPanel::onShutdown() {
  stopPlayback();

  // Clean up probing
  m_probeQueue.clear();
  m_isProbing = false;
  if (m_probePlayer) {
    m_probePlayer->stop();
  }
}

void NMVoiceManagerPanel::onUpdate([[maybe_unused]] double deltaTime) {
  // Qt Multimedia handles position updates via signals, no manual update needed
}

void NMVoiceManagerPanel::setupUI() {
  auto *mainWidget = new QWidget(this);
  setContentWidget(mainWidget);
  auto *mainLayout = new QVBoxLayout(mainWidget);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(4);

  setupToolBar();
  mainLayout->addWidget(m_toolbar);

  setupFilterBar();

  // Main content - tree view of voice lines
  setupVoiceList();
  mainLayout->addWidget(m_voiceTree, 1);

  // Preview/playback bar
  setupPreviewBar();

  // Statistics bar
  m_statsLabel = new QLabel(mainWidget);
  m_statsLabel->setStyleSheet("padding: 4px; color: #888;");
  m_statsLabel->setText(tr("0 dialogue lines, 0 matched, 0 unmatched"));
  mainLayout->addWidget(m_statsLabel);
}

void NMVoiceManagerPanel::setupToolBar() {
  m_toolbar = new QToolBar(this);
  m_toolbar->setIconSize(QSize(16, 16));

  auto *scanBtn = new QPushButton(tr("Scan"), m_toolbar);
  scanBtn->setToolTip(tr("Scan project for dialogue lines and voice files"));
  connect(scanBtn, &QPushButton::clicked, this,
          &NMVoiceManagerPanel::onScanClicked);
  m_toolbar->addWidget(scanBtn);

  auto *autoMatchBtn = new QPushButton(tr("Auto-Match"), m_toolbar);
  autoMatchBtn->setToolTip(
      tr("Automatically match voice files to dialogue lines"));
  connect(autoMatchBtn, &QPushButton::clicked, this,
          &NMVoiceManagerPanel::onAutoMatchClicked);
  m_toolbar->addWidget(autoMatchBtn);

  m_toolbar->addSeparator();

  auto *importBtn = new QPushButton(tr("Import"), m_toolbar);
  importBtn->setToolTip(tr("Import voice mapping from CSV"));
  connect(importBtn, &QPushButton::clicked, this,
          &NMVoiceManagerPanel::onImportClicked);
  m_toolbar->addWidget(importBtn);

  auto *exportBtn = new QPushButton(tr("Export"), m_toolbar);
  exportBtn->setToolTip(tr("Export voice manifest"));
  connect(exportBtn, &QPushButton::clicked, this,
          &NMVoiceManagerPanel::onExportClicked);
  m_toolbar->addWidget(exportBtn);

  auto *exportTemplateBtn = new QPushButton(tr("Export Template"), m_toolbar);
  exportTemplateBtn->setToolTip(
      tr("Export VO template for recording workflow"));
  connect(exportTemplateBtn, &QPushButton::clicked, this,
          &NMVoiceManagerPanel::onExportTemplateClicked);
  m_toolbar->addWidget(exportTemplateBtn);

  m_toolbar->addSeparator();

  auto *validateBtn = new QPushButton(tr("Validate"), m_toolbar);
  validateBtn->setToolTip(tr("Validate manifest for errors"));
  connect(validateBtn, &QPushButton::clicked, this,
          &NMVoiceManagerPanel::onValidateManifestClicked);
  m_toolbar->addWidget(validateBtn);

  m_toolbar->addSeparator();

  auto *openFolderBtn = new QPushButton(tr("Open Folder"), m_toolbar);
  openFolderBtn->setToolTip(tr("Open the voice files folder"));
  connect(openFolderBtn, &QPushButton::clicked, this,
          &NMVoiceManagerPanel::onOpenVoiceFolder);
  m_toolbar->addWidget(openFolderBtn);
}

void NMVoiceManagerPanel::setupFilterBar() {
  auto *filterWidget = new QWidget(m_toolbar);
  auto *filterLayout = new QHBoxLayout(filterWidget);
  filterLayout->setContentsMargins(4, 2, 4, 2);
  filterLayout->setSpacing(6);

  filterLayout->addWidget(new QLabel(tr("Filter:"), filterWidget));

  m_filterEdit = new QLineEdit(filterWidget);
  m_filterEdit->setPlaceholderText(tr("Search dialogue text..."));
  m_filterEdit->setMaximumWidth(200);
  connect(m_filterEdit, &QLineEdit::textChanged, this,
          &NMVoiceManagerPanel::onFilterChanged);
  filterLayout->addWidget(m_filterEdit);

  filterLayout->addWidget(new QLabel(tr("Locale:"), filterWidget));

  m_localeFilter = new QComboBox(filterWidget);
  m_localeFilter->setMinimumWidth(80);
  connect(m_localeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &NMVoiceManagerPanel::onLocaleFilterChanged);
  filterLayout->addWidget(m_localeFilter);

  filterLayout->addWidget(new QLabel(tr("Character:"), filterWidget));

  m_characterFilter = new QComboBox(filterWidget);
  m_characterFilter->addItem(tr("All Characters"));
  m_characterFilter->setMinimumWidth(120);
  connect(m_characterFilter,
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &NMVoiceManagerPanel::onCharacterFilterChanged);
  filterLayout->addWidget(m_characterFilter);

  filterLayout->addWidget(new QLabel(tr("Status:"), filterWidget));

  m_statusFilter = new QComboBox(filterWidget);
  m_statusFilter->addItem(tr("All Statuses"));
  m_statusFilter->addItem(tr("Missing"));
  m_statusFilter->addItem(tr("Recorded"));
  m_statusFilter->addItem(tr("Imported"));
  m_statusFilter->addItem(tr("Needs Review"));
  m_statusFilter->addItem(tr("Approved"));
  m_statusFilter->setMinimumWidth(100);
  connect(m_statusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &NMVoiceManagerPanel::onStatusFilterChanged);
  filterLayout->addWidget(m_statusFilter);

  m_showUnmatchedBtn = new QPushButton(tr("Unmatched Only"), filterWidget);
  m_showUnmatchedBtn->setCheckable(true);
  m_showUnmatchedBtn->setToolTip(tr("Show only lines without voice files"));
  connect(m_showUnmatchedBtn, &QPushButton::toggled, this,
          &NMVoiceManagerPanel::onShowOnlyUnmatched);
  filterLayout->addWidget(m_showUnmatchedBtn);

  filterLayout->addStretch();

  m_toolbar->addWidget(filterWidget);
}

void NMVoiceManagerPanel::setupVoiceList() {
  m_voiceTree = new QTreeWidget(this);
  m_voiceTree->setHeaderLabels({tr("Line ID"), tr("Speaker"), tr("Scene"),
                                tr("Text Key"), tr("Voice File"), tr("Takes"),
                                tr("Duration"), tr("Status"), tr("Tags")});
  m_voiceTree->setAlternatingRowColors(true);
  m_voiceTree->setSelectionMode(QAbstractItemView::SingleSelection);
  m_voiceTree->setContextMenuPolicy(Qt::CustomContextMenu);

  // Adjust column widths
  m_voiceTree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
  m_voiceTree->header()->setSectionResizeMode(3, QHeaderView::Stretch);
  m_voiceTree->setColumnWidth(0, 120); // Line ID
  m_voiceTree->setColumnWidth(1, 80);  // Speaker
  m_voiceTree->setColumnWidth(2, 80);  // Scene
  m_voiceTree->setColumnWidth(4, 150); // Voice File
  m_voiceTree->setColumnWidth(5, 50);  // Takes
  m_voiceTree->setColumnWidth(6, 60);  // Duration
  m_voiceTree->setColumnWidth(7, 80);  // Status
  m_voiceTree->setColumnWidth(8, 100); // Tags

  connect(m_voiceTree, &QTreeWidget::itemClicked, this,
          &NMVoiceManagerPanel::onLineSelected);
  connect(m_voiceTree, &QTreeWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            auto *item = m_voiceTree->itemAt(pos);
            if (!item)
              return;

            QMenu menu;
            menu.addAction(tr("Assign Voice File"), this,
                           &NMVoiceManagerPanel::onAssignVoiceFile);
            menu.addAction(tr("Clear Voice File"), this,
                           &NMVoiceManagerPanel::onClearVoiceFile);
            menu.addSeparator();
            menu.addAction(tr("Add Take"), this,
                           &NMVoiceManagerPanel::onAddTake);
            menu.addAction(tr("Set Active Take"), this,
                           &NMVoiceManagerPanel::onSetActiveTake);
            menu.addSeparator();
            menu.addAction(tr("Set Status"), this,
                           &NMVoiceManagerPanel::onSetLineStatus);
            menu.addAction(tr("Edit Metadata (Tags/Notes)"), this,
                           &NMVoiceManagerPanel::onEditLineMetadata);
            menu.addSeparator();
            menu.addAction(tr("Go to Script"), this, [this, item]() {
              QString dialogueId = item->data(0, Qt::UserRole).toString();
              emit voiceLineSelected(dialogueId);
            });
            menu.exec(m_voiceTree->mapToGlobal(pos));
          });
}

void NMVoiceManagerPanel::setupPreviewBar() {
  auto *previewWidget = new QWidget(contentWidget());
  auto *previewLayout = new QHBoxLayout(previewWidget);
  previewLayout->setContentsMargins(4, 4, 4, 4);
  previewLayout->setSpacing(6);

  m_playBtn = new QPushButton(tr("Play"), previewWidget);
  m_playBtn->setEnabled(false);
  connect(m_playBtn, &QPushButton::clicked, this,
          &NMVoiceManagerPanel::onPlayClicked);
  previewLayout->addWidget(m_playBtn);

  m_stopBtn = new QPushButton(tr("Stop"), previewWidget);
  m_stopBtn->setEnabled(false);
  connect(m_stopBtn, &QPushButton::clicked, this,
          &NMVoiceManagerPanel::onStopClicked);
  previewLayout->addWidget(m_stopBtn);

  m_playbackProgress = new QProgressBar(previewWidget);
  m_playbackProgress->setRange(0, 1000);
  m_playbackProgress->setValue(0);
  m_playbackProgress->setTextVisible(false);
  previewLayout->addWidget(m_playbackProgress, 1);

  m_durationLabel = new QLabel("0:00 / 0:00", previewWidget);
  m_durationLabel->setMinimumWidth(100);
  previewLayout->addWidget(m_durationLabel);

  previewLayout->addWidget(new QLabel(tr("Vol:"), previewWidget));

  m_volumeSlider = new QSlider(Qt::Horizontal, previewWidget);
  m_volumeSlider->setRange(0, 100);
  m_volumeSlider->setValue(100);
  m_volumeSlider->setMaximumWidth(80);
  connect(m_volumeSlider, &QSlider::valueChanged, this,
          &NMVoiceManagerPanel::onVolumeChanged);
  previewLayout->addWidget(m_volumeSlider);

  if (auto *layout = qobject_cast<QVBoxLayout *>(contentWidget()->layout())) {
    layout->addWidget(previewWidget);
  }
}

void NMVoiceManagerPanel::setupMediaPlayer() {
  // Set up audio player callbacks (issue #150 - using IAudioPlayer interface)
  if (m_audioPlayer) {
    m_audioPlayer->setVolume(static_cast<f32>(m_volume));

    m_audioPlayer->setOnPlaybackStateChanged(
        [this](AudioPlaybackState state) { onPlaybackStateChanged(); });
    m_audioPlayer->setOnMediaStatusChanged(
        [this](AudioMediaStatus status) { onMediaStatusChanged(); });
    m_audioPlayer->setOnDurationChanged(
        [this](i64 duration) { onDurationChanged(static_cast<qint64>(duration)); });
    m_audioPlayer->setOnPositionChanged(
        [this](i64 position) { onPositionChanged(static_cast<qint64>(position)); });
    m_audioPlayer->setOnError(
        [this](const std::string &error) { onMediaErrorOccurred(); });
  }

  // Create separate player for duration probing (still uses QMediaPlayer)
  m_probePlayer = new QMediaPlayer(this);

  // Connect probe player signals
  connect(m_probePlayer, &QMediaPlayer::durationChanged, this,
          &NMVoiceManagerPanel::onProbeDurationFinished);
  connect(m_probePlayer, &QMediaPlayer::errorOccurred, this,
          [this]([[maybe_unused]] QMediaPlayer::Error error,
                 [[maybe_unused]] const QString &errorString) {
            // On probe error, just move to next file
            if (VERBOSE_LOGGING) {
#ifdef QT_DEBUG
              qDebug() << "Probe error for" << m_currentProbeFile << ":"
                       << errorString;
#endif
            }
            processNextDurationProbe();
          });
}

void NMVoiceManagerPanel::scanProject() {
  // Clear and reinitialize manifest
  m_manifest = std::make_unique<NovelMind::audio::VoiceManifest>();
  m_manifest->setDefaultLocale(m_currentLocale.toStdString());

  // Add current locale if not empty
  if (!m_currentLocale.isEmpty()) {
    m_manifest->addLocale(m_currentLocale.toStdString());
  } else {
    m_currentLocale = "en";
    m_manifest->addLocale("en");
  }

  // VM-2: Set up callbacks for auto-refresh
  setupManifestCallbacks();

  m_voiceFiles.clear();
  m_durationCache.clear(); // Clear cache on full rescan

  scanScriptsForDialogue();
  scanVoiceFolder();
  autoMatchVoiceFiles();

  // Update locale filter with available locales
  if (m_localeFilter) {
    m_localeFilter->clear();
    for (const auto &locale : m_manifest->getLocales()) {
      m_localeFilter->addItem(QString::fromStdString(locale));
    }
    // Set current locale
    int currentIndex = m_localeFilter->findText(m_currentLocale);
    if (currentIndex >= 0) {
      m_localeFilter->setCurrentIndex(currentIndex);
    }
  }

  updateVoiceList();
  updateStatistics();

  // Start async duration probing for voice files
  startDurationProbing();
}

void NMVoiceManagerPanel::scanScriptsForDialogue() {
  auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    return;
  }

  QString scriptsDir =
      QString::fromStdString(pm.getFolderPath(ProjectFolder::Scripts));
  if (scriptsDir.isEmpty() || !fs::exists(scriptsDir.toStdString())) {
    return;
  }

  // Pattern to match dialogue lines: say Character "Text" or Character: "Text"
  QRegularExpression dialoguePattern(
      R"((?:say\s+)?(\w+)\s*[:\s]?\s*\"([^\"]+)\")");

  // Track speakers for character filter
  QStringList speakers;

  for (const auto &entry :
       fs::recursive_directory_iterator(scriptsDir.toStdString())) {
    if (!entry.is_regular_file() || entry.path().extension() != ".nms") {
      continue;
    }

    QString scriptPath = QString::fromStdString(entry.path().string());
    QString relPath =
        QString::fromStdString(pm.toRelativePath(entry.path().string()));

    // Extract scene name from file path
    QString sceneName = QFileInfo(relPath).baseName();

    QFile file(scriptPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      continue;
    }

    QTextStream in(&file);
    int lineNum = 0;
    while (!in.atEnd()) {
      QString line = in.readLine();
      lineNum++;

      QRegularExpressionMatch match = dialoguePattern.match(line);
      if (match.hasMatch()) {
        QString speaker = match.captured(1);
        QString dialogueText = match.captured(2);
        QString dialogueId = generateDialogueId(relPath, lineNum);

        // Create VoiceManifestLine
        NovelMind::audio::VoiceManifestLine manifestLine;
        manifestLine.id = dialogueId.toStdString();
        manifestLine.textKey =
            dialogueId.toStdString(); // Use ID as text key for now
        manifestLine.speaker = speaker.toStdString();
        manifestLine.scene = sceneName.toStdString();
        manifestLine.sourceScript = relPath.toStdString();
        manifestLine.sourceLine = static_cast<NovelMind::u32>(lineNum);

        // Add to manifest
        m_manifest->addLine(manifestLine);

        if (!speakers.contains(speaker)) {
          speakers.append(speaker);
        }
      }
    }
    file.close();
  }

  // Update character filter
  if (m_characterFilter) {
    m_characterFilter->clear();
    m_characterFilter->addItem(tr("All Characters"));
    m_characterFilter->addItems(speakers);
  }
}

void NMVoiceManagerPanel::scanVoiceFolder() {
  auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    return;
  }

  QString voiceDir =
      QString::fromStdString(pm.getProjectPath()) + "/Assets/Voice";
  if (!fs::exists(voiceDir.toStdString())) {
    return;
  }

  for (const auto &entry :
       fs::recursive_directory_iterator(voiceDir.toStdString())) {
    if (!entry.is_regular_file()) {
      continue;
    }

    QString ext =
        QString::fromStdString(entry.path().extension().string()).toLower();
    if (ext == ".ogg" || ext == ".wav" || ext == ".mp3" || ext == ".flac") {
      m_voiceFiles.append(QString::fromStdString(entry.path().string()));
    }
  }
}

void NMVoiceManagerPanel::autoMatchVoiceFiles() {
  for (const QString &voiceFile : m_voiceFiles) {
    matchVoiceToDialogue(voiceFile);
  }
}

void NMVoiceManagerPanel::matchVoiceToDialogue(const QString &voiceFile) {
  QString filename = QFileInfo(voiceFile).baseName();

  // Try exact match with dialogue ID
  auto *line = m_manifest->getLineMutable(filename.toStdString());
  if (line) {
    auto &localeFile = line->getOrCreateFile(m_currentLocale.toStdString());
    localeFile.filePath = voiceFile.toStdString();
    localeFile.status = NovelMind::audio::VoiceLineStatus::Imported;
    return;
  }

  // Try pattern matching: character_linenum or scene_linenum
  QRegularExpression pattern(R"((\w+)_(\d+))");
  QRegularExpressionMatch match = pattern.match(filename);
  if (match.hasMatch()) {
    QString prefix = match.captured(1);
    int lineNum = match.captured(2).toInt();

    // Search through all lines for matching speaker or scene
    for (const auto &manifestLine : m_manifest->getLines()) {
      if ((QString::fromStdString(manifestLine.speaker)
                   .compare(prefix, Qt::CaseInsensitive) == 0 ||
           QString::fromStdString(manifestLine.scene)
                   .compare(prefix, Qt::CaseInsensitive) == 0) &&
          manifestLine.sourceLine == static_cast<NovelMind::u32>(lineNum)) {

        // Check if already has file for this locale
        if (!manifestLine.hasFile(m_currentLocale.toStdString())) {
          auto *mutableLine = m_manifest->getLineMutable(manifestLine.id);
          if (mutableLine) {
            auto &localeFile =
                mutableLine->getOrCreateFile(m_currentLocale.toStdString());
            localeFile.filePath = voiceFile.toStdString();
            localeFile.status = NovelMind::audio::VoiceLineStatus::Imported;
            return;
          }
        }
      }
    }
  }
}

QString NMVoiceManagerPanel::generateDialogueId(const QString &scriptPath,
                                                int lineNumber) const {
  QString baseName = QFileInfo(scriptPath).baseName();
  return QString("%1_%2").arg(baseName).arg(lineNumber);
}

/**
 * @brief Filter and display voice lines with optimized O(n) performance
 *
 * PERF-1 optimization: Pre-filter lines in a single pass, then display.
 * This avoids O(n²) complexity from nested loops in filtering.
 */
void NMVoiceManagerPanel::updateVoiceList() {
  if (!m_voiceTree || !m_manifest) {
    return;
  }

  // VM-4: Save current selection before clearing
  QString selectedLineId;
  auto *currentItem = m_voiceTree->currentItem();
  if (currentItem) {
    selectedLineId = currentItem->data(0, Qt::UserRole).toString();
  }

  m_voiceTree->clear();

  // Cache filter settings once (avoid repeated calls during iteration)
  const QString filter =
      m_filterEdit ? m_filterEdit->text().trimmed().toLower() : QString();
  const QString charFilter =
      m_characterFilter && m_characterFilter->currentIndex() > 0
          ? m_characterFilter->currentText()
          : QString();
  const bool showUnmatched =
      m_showUnmatchedBtn && m_showUnmatchedBtn->isChecked();

  // Get status filter (0 = All, 1 = Missing, 2 = Recorded, 3 = Imported, 4 =
  // NeedsReview, 5 = Approved)
  const int statusFilterIndex =
      m_statusFilter ? m_statusFilter->currentIndex() : 0;
  const bool filterByStatus = (statusFilterIndex > 0);
  const NovelMind::audio::VoiceLineStatus statusFilter =
      filterByStatus ? static_cast<NovelMind::audio::VoiceLineStatus>(
                           statusFilterIndex - 1)
                     : NovelMind::audio::VoiceLineStatus::Missing;

  const std::string currentLocale = m_currentLocale.toStdString();
  const auto &lines = m_manifest->getLines();

  // Pre-allocate filtered results list to avoid repeated allocations
  struct FilteredLine {
    const NovelMind::audio::VoiceManifestLine *line;
    const NovelMind::audio::VoiceLocaleFile *localeFile;
    NovelMind::audio::VoiceLineStatus status;
    bool hasFile;
  };
  std::vector<FilteredLine> filteredLines;
  filteredLines.reserve(lines.size()); // Pre-allocate for worst case

  // Single-pass filtering: O(n) complexity
  for (const auto &line : lines) {
    // Get locale file info first (needed for multiple filter checks)
    const auto *localeFile = line.getFile(currentLocale);
    const NovelMind::audio::VoiceLineStatus status =
        localeFile ? localeFile->status
                   : NovelMind::audio::VoiceLineStatus::Missing;
    const bool hasFile = localeFile && !localeFile->filePath.empty();

    // Apply unmatched filter (early exit)
    if (showUnmatched && hasFile) {
      continue;
    }

    // Apply status filter (early exit)
    if (filterByStatus && status != statusFilter) {
      continue;
    }

    // Apply character filter (early exit)
    if (!charFilter.isEmpty()) {
      const QString speaker = QString::fromStdString(line.speaker);
      if (speaker.compare(charFilter, Qt::CaseInsensitive) != 0) {
        continue;
      }
    }

    // Apply text filter using cached toLower() (most expensive, do last)
    if (!filter.isEmpty()) {
      // Cache toLower() results to avoid repeated allocations in hot path
      const QString lineIdLower = QString::fromStdString(line.id).toLower();
      const QString speakerLower =
          QString::fromStdString(line.speaker).toLower();
      const QString textKeyLower =
          QString::fromStdString(line.textKey).toLower();

      if (!lineIdLower.contains(filter) && !speakerLower.contains(filter) &&
          !textKeyLower.contains(filter)) {
        continue;
      }
    }

    // Line passed all filters
    filteredLines.push_back({&line, localeFile, status, hasFile});
  }

  // Batch UI update: set row count first to minimize reallocations
  // (QTreeWidget handles this internally, but we're adding items efficiently)
  for (const auto &filtered : filteredLines) {
    const auto &line = *filtered.line;
    const auto *localeFile = filtered.localeFile;
    const auto status = filtered.status;

    const QString lineId = QString::fromStdString(line.id);
    const QString speaker = QString::fromStdString(line.speaker);
    const QString scene = QString::fromStdString(line.scene);
    const QString textKey = QString::fromStdString(line.textKey);

    auto *item = new QTreeWidgetItem(m_voiceTree);
    item->setData(0, Qt::UserRole, lineId);

    // Column 0: Line ID
    item->setText(0, lineId);

    // Column 1: Speaker
    item->setText(1, speaker);

    // Column 2: Scene
    item->setText(2, scene);

    // Column 3: Text Key
    item->setText(3, textKey);

    // Column 4: Voice File
    if (localeFile && !localeFile->filePath.empty()) {
      const QString filePath = QString::fromStdString(localeFile->filePath);
      item->setText(4, QFileInfo(filePath).fileName());
      item->setToolTip(4, filePath);
    } else {
      item->setText(4, tr("(none)"));
    }

    // Column 5: Takes count
    if (localeFile && !localeFile->takes.empty()) {
      item->setText(5, QString::number(localeFile->takes.size()));
    } else {
      item->setText(5, QStringLiteral("0"));
    }

    // Column 6: Duration
    if (localeFile && localeFile->duration > 0) {
      const qint64 durationMs =
          static_cast<qint64>(localeFile->duration * 1000);
      item->setText(6, formatDuration(durationMs));
    }

    // Column 7: Status (use static status text to avoid repeated allocations)
    QString statusText;
    QColor statusColor;
    switch (status) {
    case NovelMind::audio::VoiceLineStatus::Missing:
      statusText = tr("Missing");
      statusColor = QColor(200, 60, 60);
      break;
    case NovelMind::audio::VoiceLineStatus::Recorded:
      statusText = tr("Recorded");
      statusColor = QColor(60, 180, 200);
      break;
    case NovelMind::audio::VoiceLineStatus::Imported:
      statusText = tr("Imported");
      statusColor = QColor(200, 180, 60);
      break;
    case NovelMind::audio::VoiceLineStatus::NeedsReview:
      statusText = tr("Needs Review");
      statusColor = QColor(200, 120, 60);
      break;
    case NovelMind::audio::VoiceLineStatus::Approved:
      statusText = tr("Approved");
      statusColor = QColor(60, 180, 60);
      break;
    }
    item->setText(7, statusText);
    item->setForeground(7, statusColor);

    // Column 8: Tags
    if (!line.tags.empty()) {
      QStringList tagList;
      tagList.reserve(static_cast<int>(line.tags.size()));
      for (const auto &tag : line.tags) {
        tagList.append(QString::fromStdString(tag));
      }
      const QString tagsJoined = tagList.join(QStringLiteral(", "));
      item->setText(8, tagsJoined);
      item->setToolTip(8, tagsJoined);
    }
  }

  // VM-4: Restore selection if line still visible
  if (!selectedLineId.isEmpty()) {
    int row = findRowByLineId(selectedLineId);
    if (row >= 0) {
      m_voiceTree->setCurrentItem(m_voiceTree->topLevelItem(row));
    }
  }
}

void NMVoiceManagerPanel::updateStatistics() {
  if (!m_statsLabel || !m_manifest) {
    return;
  }

  // Use VoiceManifest's built-in coverage stats
  auto stats = m_manifest->getCoverageStats(m_currentLocale.toStdString());

  m_statsLabel->setText(tr("%1 lines | %2 recorded, %3 imported, %4 approved, "
                           "%5 needs review, %6 missing | %.1f%% coverage")
                            .arg(stats.totalLines)
                            .arg(stats.recordedLines)
                            .arg(stats.importedLines)
                            .arg(stats.approvedLines)
                            .arg(stats.needsReviewLines)
                            .arg(stats.missingLines)
                            .arg(stats.coveragePercent));
}

std::vector<const NovelMind::audio::VoiceManifestLine *>
NMVoiceManagerPanel::getMissingLines() const {
  if (!m_manifest) {
    return {};
  }
  return m_manifest->getLinesByStatus(
      NovelMind::audio::VoiceLineStatus::Missing,
      m_currentLocale.toStdString());
}

QStringList NMVoiceManagerPanel::getUnmatchedLines() const {
  if (!m_manifest) {
    return QStringList();
  }

  QStringList unmatchedLines;
  const std::string locale = m_currentLocale.toStdString();

  // Iterate through all lines and find those without voice files
  for (const auto &line : m_manifest->getLines()) {
    // Check if this line has a file for the current locale
    auto it = line.files.find(locale);
    if (it == line.files.end() || it->second.filePath.empty()) {
      // No file for this locale - it's unmatched
      unmatchedLines.append(QString::fromStdString(line.id));
    } else {
      // Check if the file actually exists on disk
      const QString filePath = QString::fromStdString(it->second.filePath);
      if (!QFile::exists(filePath)) {
        // File is referenced but doesn't exist - also unmatched
        unmatchedLines.append(QString::fromStdString(line.id));
      }
    }
  }

  return unmatchedLines;
}

bool NMVoiceManagerPanel::exportToCsv(const QString &filePath) {
  if (!m_manifest) {
    return false;
  }

  // Use VoiceManifest's built-in export functionality
  auto result = m_manifest->exportToCsv(filePath.toStdString(),
                                        m_currentLocale.toStdString());
  return result.isOk();
}

bool NMVoiceManagerPanel::importFromCsv(const QString &filePath) {
  if (!m_manifest) {
    return false;
  }

  // Use VoiceManifest's built-in import functionality
  auto result = m_manifest->importFromCsv(filePath.toStdString(),
                                          m_currentLocale.toStdString());
  if (result.isOk()) {
    updateVoiceList();
    updateStatistics();
    return true;
  }
  return false;
}

bool NMVoiceManagerPanel::playVoiceFile(const QString &filePath) {
  if (filePath.isEmpty()) {
    setPlaybackError(tr("No file specified"));
    return false;
  }

  if (!QFile::exists(filePath)) {
    setPlaybackError(tr("File not found: %1").arg(filePath));
    return false;
  }

  if (!m_audioPlayer) {
    setPlaybackError(tr("Audio player not available"));
    return false;
  }

  // Stop any current playback first
  stopPlayback();

  m_currentlyPlayingFile = filePath;

  // Set the source and play using IAudioPlayer interface (issue #150)
  m_audioPlayer->load(filePath.toStdString());
  m_audioPlayer->play();

  m_isPlaying = true;

  if (m_playBtn)
    m_playBtn->setEnabled(false);
  if (m_stopBtn)
    m_stopBtn->setEnabled(true);

  return true;
}

void NMVoiceManagerPanel::stopPlayback() {
  if (m_audioPlayer) {
    m_audioPlayer->stop();
    m_audioPlayer->clearSource(); // Clear source
  }

  m_isPlaying = false;
  m_currentlyPlayingFile.clear();
  m_currentDuration = 0;

  resetPlaybackUI();
}

void NMVoiceManagerPanel::resetPlaybackUI() {
  if (m_playBtn)
    m_playBtn->setEnabled(true);
  if (m_stopBtn)
    m_stopBtn->setEnabled(false);
  if (m_playbackProgress) {
    m_playbackProgress->setValue(0);
    m_playbackProgress->setRange(0, 1000); // Reset to default range
  }
  if (m_durationLabel)
    m_durationLabel->setText("0:00 / 0:00");
}

void NMVoiceManagerPanel::setPlaybackError(const QString &message) {
  if (VERBOSE_LOGGING) {
#ifdef QT_DEBUG
    qDebug() << "Playback error:" << message;
#endif
  }

  // Update status label with error
  if (m_statsLabel) {
    m_statsLabel->setStyleSheet("padding: 4px; color: #c44;");
    m_statsLabel->setText(tr("Error: %1").arg(message));

    // Reset style after 3 seconds
    QTimer::singleShot(3000, this, [this]() {
      if (m_statsLabel) {
        m_statsLabel->setStyleSheet("padding: 4px; color: #888;");
        updateStatistics();
      }
    });
  }

  emit playbackError(message);
}

QString NMVoiceManagerPanel::formatDuration(qint64 ms) const {
  int totalSeconds = static_cast<int>(ms / 1000);
  int minutes = totalSeconds / 60;
  int seconds = totalSeconds % 60;
  int tenths = static_cast<int>((ms % 1000) / 100);

  return QString("%1:%2.%3")
      .arg(minutes)
      .arg(seconds, 2, 10, QChar('0'))
      .arg(tenths);
}

// Audio player signal handlers (using IAudioPlayer interface - issue #150)
void NMVoiceManagerPanel::onPlaybackStateChanged() {
  if (!m_audioPlayer)
    return;

  AudioPlaybackState state = m_audioPlayer->getPlaybackState();

  switch (state) {
  case AudioPlaybackState::Stopped:
    m_isPlaying = false;
    // Only reset UI if we're not switching tracks
    if (m_currentlyPlayingFile.isEmpty()) {
      resetPlaybackUI();
    }
    break;

  case AudioPlaybackState::Playing:
    m_isPlaying = true;
    if (m_playBtn)
      m_playBtn->setEnabled(false);
    if (m_stopBtn)
      m_stopBtn->setEnabled(true);
    break;

  case AudioPlaybackState::Paused:
    // Currently not exposing pause, but handle it gracefully
    break;
  }
}

void NMVoiceManagerPanel::onMediaStatusChanged() {
  if (!m_audioPlayer)
    return;

  AudioMediaStatus status = m_audioPlayer->getMediaStatus();

  switch (status) {
  case AudioMediaStatus::EndOfMedia:
    // Playback finished naturally
    m_currentlyPlayingFile.clear();
    resetPlaybackUI();
    break;

  case AudioMediaStatus::InvalidMedia:
    setPlaybackError(tr("Invalid or unsupported media format"));
    m_currentlyPlayingFile.clear();
    resetPlaybackUI();
    break;

  case AudioMediaStatus::NoMedia:
  case AudioMediaStatus::Loading:
  case AudioMediaStatus::Loaded:
  case AudioMediaStatus::Stalled:
  case AudioMediaStatus::Buffering:
  case AudioMediaStatus::Buffered:
    // Normal states, no action needed
    break;
  }
}

void NMVoiceManagerPanel::onDurationChanged(qint64 duration) {
  m_currentDuration = duration;

  if (m_playbackProgress && duration > 0) {
    m_playbackProgress->setRange(0, static_cast<int>(duration));
  }

  // Update duration label
  if (m_durationLabel) {
    qint64 position = m_audioPlayer ? static_cast<qint64>(m_audioPlayer->getPositionMs()) : 0;
    m_durationLabel->setText(QString("%1 / %2")
                                 .arg(formatDuration(position))
                                 .arg(formatDuration(duration)));
  }
}

void NMVoiceManagerPanel::onPositionChanged(qint64 position) {
  if (m_playbackProgress && m_currentDuration > 0) {
    m_playbackProgress->setValue(static_cast<int>(position));
  }

  if (m_durationLabel) {
    m_durationLabel->setText(QString("%1 / %2")
                                 .arg(formatDuration(position))
                                 .arg(formatDuration(m_currentDuration)));
  }
}

void NMVoiceManagerPanel::onMediaErrorOccurred() {
  if (!m_audioPlayer)
    return;

  QString errorMsg = QString::fromStdString(m_audioPlayer->getErrorString());
  if (errorMsg.isEmpty()) {
    errorMsg = tr("Unknown playback error");
  }

  setPlaybackError(errorMsg);
  m_currentlyPlayingFile.clear();
  resetPlaybackUI();
}

// Async duration probing
void NMVoiceManagerPanel::startDurationProbing() {
  if (!m_manifest) {
    return;
  }

  // RACE-1 fix: Stop any in-progress probing before restarting
  if (m_isProbing) {
    // Cancel current probe by clearing the queue and resetting state
    m_probeQueue.clear();
    m_isProbing = false;
    m_currentProbeFile.clear();
    if (m_probePlayer) {
      m_probePlayer->stop();
    }
  }

  // Clear the probe queue and add all voice files for current locale
  m_probeQueue.clear();

  for (const auto &line : m_manifest->getLines()) {
    auto *localeFile = line.getFile(m_currentLocale.toStdString());
    if (localeFile && !localeFile->filePath.empty()) {
      QString filePath = QString::fromStdString(localeFile->filePath);
      // Check if we have a valid cached duration
      double cached = getCachedDuration(filePath);
      if (cached <= 0) {
        m_probeQueue.enqueue(filePath);
      }
    }
  }

  // Start processing
  if (!m_probeQueue.isEmpty() && !m_isProbing) {
    processNextDurationProbe();
  }
}

void NMVoiceManagerPanel::processNextDurationProbe() {
  if (m_probeQueue.isEmpty()) {
    m_isProbing = false;
    m_currentProbeFile.clear();
    // Update the list with newly probed durations
    updateDurationsInList();
    return;
  }

  m_isProbing = true;
  m_currentProbeFile = m_probeQueue.dequeue();

  if (!m_probePlayer) {
    m_isProbing = false;
    return;
  }

  m_probePlayer->setSource(QUrl::fromLocalFile(m_currentProbeFile));
}

void NMVoiceManagerPanel::onProbeDurationFinished() {
  if (!m_probePlayer || m_currentProbeFile.isEmpty() || !m_manifest) {
    processNextDurationProbe();
    return;
  }

  qint64 durationMs = m_probePlayer->duration();
  if (durationMs > 0) {
    double durationSec = static_cast<double>(durationMs) / 1000.0;
    cacheDuration(m_currentProbeFile, durationSec);

    // Update the duration in manifest for current locale
    std::string currentFilePath = m_currentProbeFile.toStdString();
    for (auto &line : m_manifest->getLines()) {
      auto *localeFile = line.getFile(m_currentLocale.toStdString());
      if (localeFile && localeFile->filePath == currentFilePath) {
        // Get mutable line to update
        auto *mutableLine = m_manifest->getLineMutable(line.id);
        if (mutableLine) {
          auto &mutableLocaleFile =
              mutableLine->getOrCreateFile(m_currentLocale.toStdString());
          mutableLocaleFile.duration = static_cast<float>(durationSec);
        }
        break;
      }
    }
  }

  // Continue with next file
  processNextDurationProbe();
}

double NMVoiceManagerPanel::getCachedDuration(const QString &filePath) const {
  auto it = m_durationCache.find(filePath.toStdString());
  if (it == m_durationCache.end()) {
    return 0.0;
  }

  // Check if file has been modified
  QFileInfo fileInfo(filePath);
  if (!fileInfo.exists()) {
    return 0.0;
  }

  qint64 currentMtime = fileInfo.lastModified().toMSecsSinceEpoch();
  if (currentMtime != it->second.mtime) {
    return 0.0; // Cache invalidated
  }

  return it->second.duration;
}

void NMVoiceManagerPanel::cacheDuration(const QString &filePath,
                                        double duration) {
  QFileInfo fileInfo(filePath);
  DurationCacheEntry entry;
  entry.duration = duration;
  entry.mtime = fileInfo.lastModified().toMSecsSinceEpoch();

  m_durationCache[filePath.toStdString()] = entry;
}

void NMVoiceManagerPanel::updateDurationsInList() {
  if (!m_voiceTree || !m_manifest)
    return;

  // Update durations in the tree widget items
  for (int i = 0; i < m_voiceTree->topLevelItemCount(); ++i) {
    auto *item = m_voiceTree->topLevelItem(i);
    if (!item)
      continue;

    QString dialogueId = item->data(0, Qt::UserRole).toString();
    auto *line = m_manifest->getLine(dialogueId.toStdString());
    if (line) {
      auto *localeFile = line->getFile(m_currentLocale.toStdString());
      if (localeFile && localeFile->duration > 0) {
        qint64 durationMs = static_cast<qint64>(localeFile->duration * 1000);
        item->setText(6, formatDuration(durationMs)); // Column 6 is duration
      }
    }
  }
}

void NMVoiceManagerPanel::onScanClicked() { scanProject(); }

void NMVoiceManagerPanel::onAutoMatchClicked() {
  autoMatchVoiceFiles();
  updateVoiceList();
  updateStatistics();
}

void NMVoiceManagerPanel::onImportClicked() {
  QString filePath = NMFileDialog::getOpenFileName(
      this, tr("Import Voice Mapping"), QString(), tr("CSV Files (*.csv)"));
  if (!filePath.isEmpty()) {
    importFromCsv(filePath);
  }
}

void NMVoiceManagerPanel::onExportClicked() {
  QString filePath = NMFileDialog::getSaveFileName(
      this, tr("Export Voice Mapping"), "voice_mapping.csv",
      tr("CSV Files (*.csv)"));
  if (!filePath.isEmpty()) {
    exportToCsv(filePath);
  }
}

void NMVoiceManagerPanel::onPlayClicked() {
  auto *item = m_voiceTree->currentItem();
  if (!item || !m_manifest) {
    return;
  }

  QString dialogueId = item->data(0, Qt::UserRole).toString();
  auto *line = m_manifest->getLine(dialogueId.toStdString());
  if (line) {
    auto *localeFile = line->getFile(m_currentLocale.toStdString());
    if (localeFile && !localeFile->filePath.empty()) {
      playVoiceFile(QString::fromStdString(localeFile->filePath));
    }
  }
}

void NMVoiceManagerPanel::onStopClicked() { stopPlayback(); }

void NMVoiceManagerPanel::onLineSelected(QTreeWidgetItem *item, int column) {
  Q_UNUSED(column);
  if (!item || !m_manifest) {
    return;
  }

  QString dialogueId = item->data(0, Qt::UserRole).toString();

  // Stop current playback when selecting a new line
  if (m_isPlaying) {
    stopPlayback();
  }

  auto *line = m_manifest->getLine(dialogueId.toStdString());
  if (line && line->hasFile(m_currentLocale.toStdString())) {
    m_playBtn->setEnabled(true);
  } else {
    m_playBtn->setEnabled(false);
  }

  emit voiceLineSelected(dialogueId);
}

void NMVoiceManagerPanel::onFilterChanged(const QString &text) {
  Q_UNUSED(text);
  updateVoiceList();
}

void NMVoiceManagerPanel::onCharacterFilterChanged(int index) {
  Q_UNUSED(index);
  updateVoiceList();
}

void NMVoiceManagerPanel::onShowOnlyUnmatched(bool checked) {
  Q_UNUSED(checked);
  updateVoiceList();
}

void NMVoiceManagerPanel::onVolumeChanged(int value) {
  m_volume = value / 100.0;
  if (m_audioPlayer) {
    m_audioPlayer->setVolume(static_cast<f32>(m_volume));
  }
}

void NMVoiceManagerPanel::onAssignVoiceFile() {
  auto *item = m_voiceTree->currentItem();
  if (!item || !m_manifest) {
    return;
  }

  QString dialogueId = item->data(0, Qt::UserRole).toString();
  auto *line = m_manifest->getLineMutable(dialogueId.toStdString());
  if (!line) {
    return;
  }

  auto &pm = ProjectManager::instance();
  QString voiceDir =
      QString::fromStdString(pm.getProjectPath()) + "/Assets/Voice";

  QString filePath = NMFileDialog::getOpenFileName(
      this, tr("Select Voice File"), voiceDir,
      tr("Audio Files (*.ogg *.wav *.mp3 *.flac)"));

  if (!filePath.isEmpty()) {
    // Assign file to current locale
    auto &localeFile = line->getOrCreateFile(m_currentLocale.toStdString());
    localeFile.filePath = filePath.toStdString();
    localeFile.status = NovelMind::audio::VoiceLineStatus::Imported;

    // Queue duration probe for this file
    if (!m_probeQueue.contains(filePath)) {
      m_probeQueue.enqueue(filePath);
      if (!m_isProbing) {
        processNextDurationProbe();
      }
    }

    updateVoiceList();
    updateStatistics();
    emit voiceFileChanged(dialogueId, filePath);
  }
}

void NMVoiceManagerPanel::onClearVoiceFile() {
  auto *item = m_voiceTree->currentItem();
  if (!item || !m_manifest) {
    return;
  }

  QString dialogueId = item->data(0, Qt::UserRole).toString();
  auto *line = m_manifest->getLineMutable(dialogueId.toStdString());
  if (!line) {
    return;
  }

  // Clear file for current locale
  auto &localeFile = line->getOrCreateFile(m_currentLocale.toStdString());
  localeFile.filePath.clear();
  localeFile.status = NovelMind::audio::VoiceLineStatus::Missing;
  localeFile.duration = 0.0f;
  localeFile.takes.clear();

  updateVoiceList();
  updateStatistics();
  emit voiceFileChanged(dialogueId, QString());
}

void NMVoiceManagerPanel::onOpenVoiceFolder() {
  auto &pm = ProjectManager::instance();
  if (!pm.hasOpenProject()) {
    return;
  }

  QString voiceDir =
      QString::fromStdString(pm.getProjectPath()) + "/Assets/Voice";

  if (!fs::exists(voiceDir.toStdString())) {
    std::error_code ec;
    fs::create_directories(voiceDir.toStdString(), ec);
  }

  QUrl url = QUrl::fromLocalFile(voiceDir);
  QDesktopServices::openUrl(url);
}

void NMVoiceManagerPanel::onLocaleFilterChanged(int index) {
  if (!m_localeFilter) {
    return;
  }

  Q_UNUSED(index);
  m_currentLocale = m_localeFilter->currentText();
  updateVoiceList();
  updateStatistics();
}

void NMVoiceManagerPanel::onStatusFilterChanged(int index) {
  Q_UNUSED(index);
  updateVoiceList();
}

void NMVoiceManagerPanel::onExportTemplateClicked() {
  if (!m_manifest) {
    return;
  }

  QString filePath = NMFileDialog::getSaveFileName(
      this, tr("Export VO Template"), "vo_template.json",
      tr("JSON Files (*.json)"));

  if (!filePath.isEmpty()) {
    auto result = m_manifest->exportTemplate(filePath.toStdString());
    if (result.isOk()) {
      m_statsLabel->setText(tr("Template exported successfully"));
      QTimer::singleShot(3000, this, [this]() { updateStatistics(); });
    } else {
      m_statsLabel->setText(tr("Failed to export template: %1")
                                .arg(QString::fromStdString(result.error())));
      QTimer::singleShot(3000, this, [this]() { updateStatistics(); });
    }
  }
}

void NMVoiceManagerPanel::onValidateManifestClicked() {
  if (!m_manifest) {
    return;
  }

  auto errors = m_manifest->validate(true); // Check files on disk

  if (errors.empty()) {
    m_statsLabel->setText(tr("Validation passed - no errors found"));
    QTimer::singleShot(3000, this, [this]() { updateStatistics(); });
  } else {
    // Show validation errors in a message box
    QStringList errorMessages;
    for (const auto &error : errors) {
      QString errorMsg = QString("Line %1: %2")
                             .arg(QString::fromStdString(error.lineId))
                             .arg(QString::fromStdString(error.message));
      errorMessages.append(errorMsg);
    }

    m_statsLabel->setText(
        tr("Validation found %1 error(s)").arg(errors.size()));

    // Show first 10 errors in a simple dialog
    QString msg =
        tr("Validation Errors:\n\n") + errorMessages.mid(0, 10).join("\n");
    if (errors.size() > 10) {
      msg += tr("\n\n... and %1 more errors").arg(errors.size() - 10);
    }

    NMMessageDialog::showWarning(this, tr("Validation Errors"), msg);
  }
}

void NMVoiceManagerPanel::onEditLineMetadata() {
  auto *item = m_voiceTree->currentItem();
  if (!item || !m_manifest) {
    return;
  }

  QString dialogueId = item->data(0, Qt::UserRole).toString();
  auto *line = m_manifest->getLineMutable(dialogueId.toStdString());
  if (!line) {
    return;
  }

  // Collect current tags
  QStringList currentTags;
  for (const auto &tag : line->tags) {
    currentTags.append(QString::fromStdString(tag));
  }

  // Get available speakers and scenes from manifest
  QStringList availableSpeakers;
  QStringList availableScenes;
  QStringList suggestedTags;

  for (const auto &speaker : m_manifest->getSpeakers()) {
    availableSpeakers.append(QString::fromStdString(speaker));
  }
  for (const auto &scene : m_manifest->getScenes()) {
    availableScenes.append(QString::fromStdString(scene));
  }
  for (const auto &tag : m_manifest->getTags()) {
    suggestedTags.append(QString::fromStdString(tag));
  }

  // Use the proper metadata dialog
  NMVoiceMetadataDialog::MetadataResult result;
  bool ok = NMVoiceMetadataDialog::getMetadata(
      this, dialogueId, currentTags, QString::fromStdString(line->notes),
      QString::fromStdString(line->speaker),
      QString::fromStdString(line->scene), result, availableSpeakers,
      availableScenes, suggestedTags);

  if (ok) {
    // Update tags
    line->tags.clear();
    for (const QString &tag : result.tags) {
      line->tags.push_back(tag.toStdString());
    }

    // Update other metadata
    line->notes = result.notes.toStdString();
    line->speaker = result.speaker.toStdString();
    line->scene = result.scene.toStdString();

    updateVoiceList();

    m_statsLabel->setText(tr("Metadata updated for line: %1").arg(dialogueId));
    QTimer::singleShot(3000, this, [this]() { updateStatistics(); });
  }
}

void NMVoiceManagerPanel::onAddTake() {
  auto *item = m_voiceTree->currentItem();
  if (!item || !m_manifest) {
    return;
  }

  QString dialogueId = item->data(0, Qt::UserRole).toString();

  auto &pm = ProjectManager::instance();
  QString voiceDir =
      QString::fromStdString(pm.getProjectPath()) + "/Assets/Voice";

  QString filePath = NMFileDialog::getOpenFileName(
      this, tr("Select Take Audio File"), voiceDir,
      tr("Audio Files (*.ogg *.wav *.mp3 *.flac)"));

  if (!filePath.isEmpty()) {
    NovelMind::audio::VoiceTake take;
    take.filePath = filePath.toStdString();
    take.recordedTimestamp =
        static_cast<NovelMind::u64>(QDateTime::currentSecsSinceEpoch());
    take.isActive = false;

    auto result = m_manifest->addTake(dialogueId.toStdString(),
                                      m_currentLocale.toStdString(), take);
    if (result.isOk()) {
      updateVoiceList();
      m_statsLabel->setText(tr("Take added successfully"));
      QTimer::singleShot(3000, this, [this]() { updateStatistics(); });
    } else {
      m_statsLabel->setText(tr("Failed to add take: %1")
                                .arg(QString::fromStdString(result.error())));
      QTimer::singleShot(3000, this, [this]() { updateStatistics(); });
    }
  }
}

void NMVoiceManagerPanel::onSetActiveTake() {
  auto *item = m_voiceTree->currentItem();
  if (!item || !m_manifest) {
    return;
  }

  QString dialogueId = item->data(0, Qt::UserRole).toString();
  auto *line = m_manifest->getLine(dialogueId.toStdString());
  if (!line) {
    return;
  }

  auto *localeFile = line->getFile(m_currentLocale.toStdString());
  if (!localeFile || localeFile->takes.empty()) {
    m_statsLabel->setText(tr("No takes available for this line"));
    QTimer::singleShot(3000, this, [this]() { updateStatistics(); });
    return;
  }

  // Show dialog to select take
  QStringList takeNames;
  for (size_t i = 0; i < localeFile->takes.size(); ++i) {
    const auto &take = localeFile->takes[i];
    QString takeName =
        QString("Take %1: %2 %3")
            .arg(take.takeNumber)
            .arg(QFileInfo(QString::fromStdString(take.filePath)).fileName())
            .arg(take.isActive ? "(active)" : "");
    takeNames.append(takeName);
  }

  bool ok;
  QString selected = NMInputDialog::getItem(
      this, tr("Select Active Take"), tr("Choose which take to set as active:"),
      takeNames, static_cast<int>(localeFile->activeTakeIndex), false, &ok);

  if (ok) {
    int selectedIndex = static_cast<int>(takeNames.indexOf(selected));
    auto result = m_manifest->setActiveTake(
        dialogueId.toStdString(), m_currentLocale.toStdString(),
        static_cast<NovelMind::u32>(selectedIndex));
    if (result.isOk()) {
      updateVoiceList();
      m_statsLabel->setText(tr("Active take set successfully"));
      QTimer::singleShot(3000, this, [this]() { updateStatistics(); });
    }
  }
}

void NMVoiceManagerPanel::onSetLineStatus() {
  auto *item = m_voiceTree->currentItem();
  if (!item || !m_manifest) {
    return;
  }

  QString dialogueId = item->data(0, Qt::UserRole).toString();

  QStringList statuses = {tr("Missing"), tr("Recorded"), tr("Imported"),
                          tr("Needs Review"), tr("Approved")};

  bool ok;
  QString selected = NMInputDialog::getItem(this, tr("Set Status"),
                                            tr("Select status for this line:"),
                                            statuses, 0, false, &ok);

  if (ok) {
    int statusIndex = static_cast<int>(statuses.indexOf(selected));
    NovelMind::audio::VoiceLineStatus status =
        static_cast<NovelMind::audio::VoiceLineStatus>(statusIndex);

    auto result = m_manifest->setStatus(dialogueId.toStdString(),
                                        m_currentLocale.toStdString(), status);
    if (result.isOk()) {
      updateVoiceList();
      updateStatistics();
    }
  }
}

// ============================================================================
// VM-2: Auto-Refresh Implementation
// ============================================================================

void NMVoiceManagerPanel::onRecordingCompleted(const QString &lineId,
                                               const QString &locale) {
  if (VERBOSE_LOGGING) {
#ifdef QT_DEBUG
    qDebug() << "Recording completed:" << lineId << "(" << locale << ")";
#endif
  }

  // Find row for this line
  int row = findRowByLineId(lineId);

  if (row >= 0) {
    // Update status column only (avoid full refresh to preserve filters)
    updateRowStatus(row, lineId, locale);

    // Flash row to indicate update
    flashRow(row);
  } else {
    // Line not visible (filtered out) - just update statistics
    // Don't refresh table as it would show the line despite active filters
  }

  // Update statistics
  updateStatistics();
}

void NMVoiceManagerPanel::onFileStatusChanged(const QString &lineId,
                                              const QString &locale) {
  if (VERBOSE_LOGGING) {
#ifdef QT_DEBUG
    qDebug() << "File status changed:" << lineId << "(" << locale << ")";
#endif
  }

  // Only update if it's for the current locale
  if (locale != m_currentLocale) {
    return;
  }

  // Find row for this line
  int row = findRowByLineId(lineId);

  if (row >= 0) {
    // Update status column only
    updateRowStatus(row, lineId, locale);

    // Flash row to indicate update
    flashRow(row);
  }

  // Update statistics
  updateStatistics();
}

// ============================================================================
// VM-4: Selection and Row Management
// ============================================================================

int NMVoiceManagerPanel::findRowByLineId(const QString &lineId) const {
  if (!m_voiceTree) {
    return -1;
  }

  for (int i = 0; i < m_voiceTree->topLevelItemCount(); ++i) {
    auto *item = m_voiceTree->topLevelItem(i);
    if (item && item->data(0, Qt::UserRole).toString() == lineId) {
      return i;
    }
  }
  return -1;
}

void NMVoiceManagerPanel::updateRowStatus(int row, const QString &lineId,
                                          const QString &locale) {
  if (!m_voiceTree || !m_manifest || row < 0 ||
      row >= m_voiceTree->topLevelItemCount()) {
    return;
  }

  auto *item = m_voiceTree->topLevelItem(row);
  if (!item) {
    return;
  }

  auto *line = m_manifest->getLine(lineId.toStdString());
  if (!line) {
    return;
  }

  std::string localeStr =
      locale.isEmpty() ? m_currentLocale.toStdString() : locale.toStdString();
  auto *localeFile = line->getFile(localeStr);

  // Update status column (column 7)
  NovelMind::audio::VoiceLineStatus status =
      localeFile ? localeFile->status
                 : NovelMind::audio::VoiceLineStatus::Missing;

  QString statusText;
  QColor statusColor;
  switch (status) {
  case NovelMind::audio::VoiceLineStatus::Missing:
    statusText = tr("Missing");
    statusColor = QColor(200, 60, 60);
    break;
  case NovelMind::audio::VoiceLineStatus::Recorded:
    statusText = tr("Recorded");
    statusColor = QColor(60, 180, 200);
    break;
  case NovelMind::audio::VoiceLineStatus::Imported:
    statusText = tr("Imported");
    statusColor = QColor(200, 180, 60);
    break;
  case NovelMind::audio::VoiceLineStatus::NeedsReview:
    statusText = tr("Needs Review");
    statusColor = QColor(200, 120, 60);
    break;
  case NovelMind::audio::VoiceLineStatus::Approved:
    statusText = tr("Approved");
    statusColor = QColor(60, 180, 60);
    break;
  }
  item->setText(7, statusText);
  item->setForeground(7, statusColor);

  // Update voice file column (column 4)
  if (localeFile && !localeFile->filePath.empty()) {
    QString filePath = QString::fromStdString(localeFile->filePath);
    item->setText(4, QFileInfo(filePath).fileName());
    item->setToolTip(4, filePath);
  } else {
    item->setText(4, tr("(none)"));
    item->setToolTip(4, QString());
  }

  // Update duration column (column 6)
  if (localeFile && localeFile->duration > 0) {
    qint64 durationMs = static_cast<qint64>(localeFile->duration * 1000);
    item->setText(6, formatDuration(durationMs));
  } else {
    item->setText(6, QString());
  }
}

void NMVoiceManagerPanel::flashRow(int row) {
  if (!m_voiceTree || row < 0 || row >= m_voiceTree->topLevelItemCount()) {
    return;
  }

  auto *item = m_voiceTree->topLevelItem(row);
  if (!item) {
    return;
  }

  // Store original background color (use first cell as reference)
  QBrush originalBrush = item->background(0);
  QColor flashColor(100, 200, 100, 100); // Light green

  // Apply flash color to all cells in row
  for (int col = 0; col < m_voiceTree->columnCount(); ++col) {
    item->setBackground(col, flashColor);
  }

  // Reset to original color after 500ms
  QTimer::singleShot(500, this, [this, row, originalBrush]() {
    if (!m_voiceTree || row >= m_voiceTree->topLevelItemCount()) {
      return;
    }
    auto *item = m_voiceTree->topLevelItem(row);
    if (item) {
      for (int col = 0; col < m_voiceTree->columnCount(); ++col) {
        item->setBackground(col, originalBrush);
      }
    }
  });
}

void NMVoiceManagerPanel::setupManifestCallbacks() {
  if (!m_manifest) {
    return;
  }

  // Set up callback for status changes
  m_manifest->setOnStatusChanged(
      [this](const std::string &lineId, const std::string &locale,
             [[maybe_unused]] NovelMind::audio::VoiceLineStatus status) {
        // Use QMetaObject::invokeMethod to ensure thread safety
        // This will post the call to the Qt event loop
        QMetaObject::invokeMethod(
            this, "onFileStatusChanged", Qt::QueuedConnection,
            Q_ARG(QString, QString::fromStdString(lineId)),
            Q_ARG(QString, QString::fromStdString(locale)));
      });

  // Set up callback for line changes
  m_manifest->setOnLineChanged([this](const std::string &lineId) {
    // Line content changed - may need to update display
    QMetaObject::invokeMethod(this, "onFileStatusChanged", Qt::QueuedConnection,
                              Q_ARG(QString, QString::fromStdString(lineId)),
                              Q_ARG(QString, m_currentLocale));
  });
}

} // namespace NovelMind::editor::qt
