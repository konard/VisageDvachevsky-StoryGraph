#include "NovelMind/editor/project_manager.hpp"
#include "NovelMind/editor/qt/nm_dialogs.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/editor/qt/panels/nm_inspector_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_scene_view_panel.hpp"
#include "NovelMind/editor/qt/panels/nm_story_graph_panel.hpp"
#include "NovelMind/editor/qt/qt_event_bus.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

class NMAssetButton : public QPushButton {
  Q_OBJECT

public:
  explicit NMAssetButton(const QString& text, QWidget* parent = nullptr)
      : QPushButton(text, parent) {
    setAcceptDrops(true);
  }

signals:
  void assetDropped(const QString& path);

protected:
  void dragEnterEvent(QDragEnterEvent* event) override {
    if (event->mimeData() && event->mimeData()->hasUrls()) {
      event->acceptProposedAction();
      return;
    }
    QPushButton::dragEnterEvent(event);
  }

  void dragMoveEvent(QDragMoveEvent* event) override {
    if (event->mimeData() && event->mimeData()->hasUrls()) {
      event->acceptProposedAction();
      return;
    }
    QPushButton::dragMoveEvent(event);
  }

  void dropEvent(QDropEvent* event) override {
    if (event->mimeData() && event->mimeData()->hasUrls()) {
      const QList<QUrl> urls = event->mimeData()->urls();
      if (!urls.isEmpty() && urls.first().isLocalFile()) {
        emit assetDropped(urls.first().toLocalFile());
        event->acceptProposedAction();
        return;
      }
    }
    QPushButton::dropEvent(event);
  }
};

namespace {

enum class ImportTargetMode { AutoByType, AssetsRoot, CustomFolder };

QString importDestinationForExtension(const QString& extension) {
  auto& projectManager = ProjectManager::instance();
  if (!projectManager.hasOpenProject()) {
    return QString();
  }

  const QString ext = extension.toLower();
  if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "gif") {
    return QString::fromStdString(projectManager.getFolderPath(ProjectFolder::Images));
  }
  if (ext == "wav" || ext == "mp3" || ext == "ogg" || ext == "flac") {
    return QString::fromStdString(projectManager.getFolderPath(ProjectFolder::Audio));
  }
  if (ext == "ttf" || ext == "otf") {
    return QString::fromStdString(projectManager.getFolderPath(ProjectFolder::Fonts));
  }
  if (ext == "nms") {
    return QString::fromStdString(projectManager.getFolderPath(ProjectFolder::Scripts));
  }
  if (ext == "nmscene") {
    return QString::fromStdString(projectManager.getFolderPath(ProjectFolder::Scenes));
  }

  return QString::fromStdString(projectManager.getFolderPath(ProjectFolder::Assets));
}

QString generateUniquePath(const QString& directory, const QString& fileName) {
  QDir dir(directory);
  QString baseName = QFileInfo(fileName).completeBaseName();
  QString suffix = QFileInfo(fileName).suffix();
  QString candidate = dir.filePath(fileName);
  int counter = 1;

  while (QFileInfo::exists(candidate)) {
    QString numbered = suffix.isEmpty()
                           ? QString("%1_%2").arg(baseName).arg(counter)
                           : QString("%1_%2.%3").arg(baseName).arg(counter).arg(suffix);
    candidate = dir.filePath(numbered);
    counter++;
  }

  return candidate;
}

struct AssetImportChoice {
  bool accepted = false;
  bool useExternal = false;
  QString targetDir;
};

AssetImportChoice promptAssetImport(QWidget* parent, const QString& assetsRoot,
                                    const QString& extension) {
  AssetImportChoice result;

  QDialog dialog(parent);
  dialog.setWindowTitle(QObject::tr("Import Asset"));
  dialog.setModal(true);
  dialog.setMinimumSize(520, 200);

  auto* layout = new QVBoxLayout(&dialog);
  layout->setContentsMargins(12, 12, 12, 12);
  layout->setSpacing(8);

  auto* title =
      new QLabel(QObject::tr("This file is outside the project. Import it into Assets?"), &dialog);
  title->setWordWrap(true);
  layout->addWidget(title);

  auto* destLabel = new QLabel(QObject::tr("Import destination"), &dialog);
  layout->addWidget(destLabel);

  auto* destRow = new QHBoxLayout();
  auto* destCombo = new QComboBox(&dialog);
  destCombo->addItem(QObject::tr("Auto by type (Images/Audio/Fonts/etc)"),
                     static_cast<int>(ImportTargetMode::AutoByType));
  destCombo->addItem(QObject::tr("Assets root"), static_cast<int>(ImportTargetMode::AssetsRoot));
  destCombo->addItem(QObject::tr("Choose folder..."),
                     static_cast<int>(ImportTargetMode::CustomFolder));

  auto* customEdit = new QLineEdit(&dialog);
  customEdit->setPlaceholderText(QObject::tr("Select a folder inside Assets"));
  customEdit->setEnabled(false);
  auto* browseButton = new QPushButton(QObject::tr("Browse"), &dialog);
  browseButton->setEnabled(false);

  destRow->addWidget(destCombo, 1);
  destRow->addWidget(customEdit, 2);
  destRow->addWidget(browseButton);
  layout->addLayout(destRow);

  auto updateCustomState = [&]() {
    const auto modeValue = static_cast<ImportTargetMode>(destCombo->currentData().toInt());
    const bool custom = (modeValue == ImportTargetMode::CustomFolder);
    customEdit->setEnabled(custom);
    browseButton->setEnabled(custom);
  };
  updateCustomState();

  QObject::connect(destCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), &dialog,
                   [&](int) { updateCustomState(); });
  QObject::connect(browseButton, &QPushButton::clicked, &dialog, [&]() {
    const QString dir = NMFileDialog::getExistingDirectory(
        &dialog, QObject::tr("Select Import Folder"), assetsRoot);
    if (!dir.isEmpty()) {
      customEdit->setText(dir);
    }
  });

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  auto* externalButton =
      buttons->addButton(QObject::tr("Use External"), QDialogButtonBox::ActionRole);
  layout->addWidget(buttons);

  QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
    const auto modeValue = static_cast<ImportTargetMode>(destCombo->currentData().toInt());
    if (modeValue == ImportTargetMode::CustomFolder) {
      const QString customDir = customEdit->text().trimmed();
      if (customDir.isEmpty()) {
        NMMessageDialog::showWarning(
            &dialog, QObject::tr("Import Asset"),
            QObject::tr("Please choose a destination folder inside Assets."));
        return;
      }
      if (!customDir.startsWith(assetsRoot)) {
        NMMessageDialog::showWarning(&dialog, QObject::tr("Import Asset"),
                                     QObject::tr("Destination must be inside the Assets folder."));
        return;
      }
    }
    dialog.accept();
  });
  QObject::connect(externalButton, &QPushButton::clicked, &dialog, [&]() {
    result.useExternal = true;
    dialog.accept();
  });

  if (dialog.exec() != QDialog::Accepted) {
    return result;
  }

  result.accepted = true;
  if (result.useExternal) {
    return result;
  }

  const auto modeValue = static_cast<ImportTargetMode>(destCombo->currentData().toInt());
  switch (modeValue) {
  case ImportTargetMode::AutoByType:
    result.targetDir = importDestinationForExtension(extension);
    break;
  case ImportTargetMode::AssetsRoot:
    result.targetDir = assetsRoot;
    break;
  case ImportTargetMode::CustomFolder:
    result.targetDir = customEdit->text().trimmed();
    break;
  }

  if (result.targetDir.isEmpty()) {
    result.targetDir = assetsRoot;
  }

  return result;
}

} // namespace

// ============================================================================
// NMPropertyGroup
// ============================================================================

NMPropertyGroup::NMPropertyGroup(const QString& title, QWidget* parent) : QWidget(parent) {
  setObjectName("InspectorGroup");
  auto* mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  // Header
  m_header = new QWidget(this);
  m_header->setObjectName("InspectorGroupHeader");
  m_header->setCursor(Qt::PointingHandCursor);
  auto* headerLayout = new QHBoxLayout(m_header);
  headerLayout->setContentsMargins(4, 4, 4, 4);

  m_expandIcon = new QLabel(QString::fromUtf8("\u25BC"), m_header); // Down arrow
  m_expandIcon->setFixedWidth(16);
  headerLayout->addWidget(m_expandIcon);

  auto* titleLabel = new QLabel(title, m_header);
  QFont boldFont = titleLabel->font();
  boldFont.setBold(true);
  titleLabel->setFont(boldFont);
  headerLayout->addWidget(titleLabel);

  headerLayout->addStretch();

  // Make header clickable
  m_header->installEventFilter(this);

  mainLayout->addWidget(m_header);

  // Separator
  auto* separator = new QFrame(this);
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Sunken);
  mainLayout->addWidget(separator);

  // Content area
  m_content = new QWidget(this);
  m_content->setObjectName("InspectorGroupContent");
  m_contentLayout = new QVBoxLayout(m_content);
  m_contentLayout->setContentsMargins(8, 4, 8, 8);
  m_contentLayout->setSpacing(4);

  mainLayout->addWidget(m_content);

  // Event filter cleanup is handled automatically by Qt when the widget is destroyed
}

void NMPropertyGroup::setExpanded(bool expanded) {
  m_expanded = expanded;
  m_content->setVisible(expanded);
  m_expandIcon->setText(expanded ? QString::fromUtf8("\u25BC") : QString::fromUtf8("\u25B6"));
}

void NMPropertyGroup::addProperty(const QString& name, const QString& value) {
  auto* row = new QWidget(m_content);
  auto* rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->setSpacing(8);

  auto* nameLabel = new QLabel(name + ":", row);
  nameLabel->setMinimumWidth(100);
  nameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  const auto& palette = NMStyleManager::instance().palette();
  nameLabel->setStyleSheet(
      QString("color: %1;").arg(NMStyleManager::colorToStyleString(palette.textSecondary)));

  auto* valueLabel = new QLabel(value, row);
  valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

  rowLayout->addWidget(nameLabel);
  rowLayout->addWidget(valueLabel, 1);

  m_contentLayout->addWidget(row);
}

void NMPropertyGroup::addProperty(const QString& name, QWidget* widget) {
  auto* row = new QWidget(m_content);
  auto* rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->setSpacing(8);

  auto* nameLabel = new QLabel(name + ":", row);
  nameLabel->setMinimumWidth(100);
  nameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  const auto& palette = NMStyleManager::instance().palette();
  nameLabel->setStyleSheet(
      QString("color: %1;").arg(NMStyleManager::colorToStyleString(palette.textSecondary)));

  rowLayout->addWidget(nameLabel);
  rowLayout->addWidget(widget, 1);

  m_contentLayout->addWidget(row);
}

void NMPropertyGroup::clearProperties() {
  QLayoutItem* item;
  while ((item = m_contentLayout->takeAt(0)) != nullptr) {
    delete item->widget();
    delete item;
  }
}

void NMPropertyGroup::onHeaderClicked() {
  setExpanded(!m_expanded);
}

bool NMPropertyGroup::eventFilter(QObject* obj, QEvent* event) {
  if (obj == m_header && event->type() == QEvent::MouseButtonRelease) {
    auto* mouseEvent = static_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      onHeaderClicked();
      return true;
    }
  }
  if (event->type() == QEvent::FocusOut) {
    if (auto* textEdit = qobject_cast<QPlainTextEdit*>(obj)) {
      const QString property = textEdit->property("propertyName").toString();
      emit propertyValueChanged(property, textEdit->toPlainText());
    }
  }
  return QWidget::eventFilter(obj, event);
}

QWidget* NMPropertyGroup::addEditableProperty(const QString& propertyName, const QString& label,
                                              NMPropertyType type, const QString& currentValue,
                                              const QStringList& enumValues) {
  QWidget* editor = nullptr;
  const auto& palette = NMStyleManager::instance().palette();

  switch (type) {
  case NMPropertyType::String: {
    auto* lineEdit = new QLineEdit(currentValue);
    lineEdit->setProperty("propertyName", propertyName);
    lineEdit->setStyleSheet(QString("QLineEdit {"
                                    "  background-color: %1;"
                                    "  color: %2;"
                                    "  border: 1px solid %3;"
                                    "  border-radius: 3px;"
                                    "  padding: 4px;"
                                    "}"
                                    "QLineEdit:focus {"
                                    "  border-color: %4;"
                                    "}")
                                .arg(palette.bgDark.name())
                                .arg(palette.textPrimary.name())
                                .arg(palette.borderDark.name())
                                .arg(palette.accentPrimary.name()));

    connect(lineEdit, &QLineEdit::editingFinished, this, &NMPropertyGroup::onPropertyEdited);
    editor = lineEdit;
    break;
  }
  case NMPropertyType::MultiLine: {
    auto* textEdit = new QPlainTextEdit(currentValue);
    textEdit->setProperty("propertyName", propertyName);
    textEdit->setTabChangesFocus(true);
    textEdit->setMinimumHeight(90);
    textEdit->setStyleSheet(QString("QPlainTextEdit {"
                                    "  background-color: %1;"
                                    "  color: %2;"
                                    "  border: 1px solid %3;"
                                    "  border-radius: 3px;"
                                    "  padding: 6px;"
                                    "}")
                                .arg(palette.bgDark.name())
                                .arg(palette.textPrimary.name())
                                .arg(palette.borderDark.name()));

    auto* debounce = new QTimer(textEdit);
    debounce->setSingleShot(true);
    debounce->setInterval(400);
    connect(textEdit, &QPlainTextEdit::textChanged, this, [debounce]() { debounce->start(); });
    connect(debounce, &QTimer::timeout, this, [this, textEdit]() {
      const QString property = textEdit->property("propertyName").toString();
      emit propertyValueChanged(property, textEdit->toPlainText());
    });
    textEdit->installEventFilter(this);

    editor = textEdit;
    break;
  }

  case NMPropertyType::Integer: {
    auto* spinBox = new QSpinBox();
    spinBox->setProperty("propertyName", propertyName);
    spinBox->setRange(-999999, 999999);
    spinBox->setValue(currentValue.toInt());
    spinBox->setStyleSheet(QString("QSpinBox {"
                                   "  background-color: %1;"
                                   "  color: %2;"
                                   "  border: 1px solid %3;"
                                   "  border-radius: 3px;"
                                   "  padding: 4px;"
                                   "}")
                               .arg(palette.bgDark.name())
                               .arg(palette.textPrimary.name())
                               .arg(palette.borderDark.name()));

    connect(spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &NMPropertyGroup::onPropertyEdited);
    editor = spinBox;
    break;
  }

  case NMPropertyType::Range:
    // Range uses float editor until range metadata is available.
    [[fallthrough]];
  case NMPropertyType::Float: {
    auto* doubleSpinBox = new QDoubleSpinBox();
    doubleSpinBox->setProperty("propertyName", propertyName);
    doubleSpinBox->setRange(-999999.0, 999999.0);
    doubleSpinBox->setDecimals(3);
    doubleSpinBox->setValue(currentValue.toDouble());
    doubleSpinBox->setStyleSheet(QString("QDoubleSpinBox {"
                                         "  background-color: %1;"
                                         "  color: %2;"
                                         "  border: 1px solid %3;"
                                         "  border-radius: 3px;"
                                         "  padding: 4px;"
                                         "}")
                                     .arg(palette.bgDark.name())
                                     .arg(palette.textPrimary.name())
                                     .arg(palette.borderDark.name()));

    connect(doubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &NMPropertyGroup::onPropertyEdited);
    editor = doubleSpinBox;
    break;
  }

  case NMPropertyType::Boolean: {
    auto* checkBox = new QCheckBox();
    checkBox->setProperty("propertyName", propertyName);
    checkBox->setChecked(currentValue.toLower() == "true" || currentValue == "1");
    checkBox->setStyleSheet(QString("QCheckBox { color: %1; }").arg(palette.textPrimary.name()));

    connect(checkBox, &QCheckBox::toggled, this, &NMPropertyGroup::onPropertyEdited);
    editor = checkBox;
    break;
  }

  case NMPropertyType::Enum: {
    auto* comboBox = new QComboBox();
    comboBox->setProperty("propertyName", propertyName);
    comboBox->addItems(enumValues);
    comboBox->setCurrentText(currentValue);
    comboBox->setStyleSheet(QString("QComboBox {"
                                    "  background-color: %1;"
                                    "  color: %2;"
                                    "  border: 1px solid %3;"
                                    "  border-radius: 3px;"
                                    "  padding: 4px;"
                                    "}"
                                    "QComboBox::drop-down {"
                                    "  border: none;"
                                    "}"
                                    "QComboBox::down-arrow {"
                                    "  image: none;"
                                    "  border: none;"
                                    "}")
                                .arg(palette.bgDark.name())
                                .arg(palette.textPrimary.name())
                                .arg(palette.borderDark.name()));

    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &NMPropertyGroup::onPropertyEdited);
    editor = comboBox;
    break;
  }

  case NMPropertyType::Color: {
    auto* colorButton = new QPushButton();
    colorButton->setProperty("propertyName", propertyName);
    colorButton->setFixedHeight(30);

    QColor color(currentValue);
    if (!color.isValid()) {
      color = Qt::white;
    }
    colorButton->setProperty("currentColor", color);

    colorButton->setStyleSheet(QString("QPushButton {"
                                       "  background-color: %1;"
                                       "  border: 1px solid %2;"
                                       "  border-radius: 3px;"
                                       "}"
                                       "QPushButton:hover {"
                                       "  border-color: %3;"
                                       "}")
                                   .arg(color.name())
                                   .arg(palette.borderDark.name())
                                   .arg(palette.accentPrimary.name()));

    connect(colorButton, &QPushButton::clicked, this, [this, colorButton, propertyName]() {
      QColor current = colorButton->property("currentColor").value<QColor>();
      bool ok = false;
      QColor newColor =
          NMColorDialog::getColor(current, this, QString("Choose %1").arg(propertyName), &ok);

      if (ok) {
        colorButton->setProperty("currentColor", newColor);
        colorButton->setStyleSheet(
            QString("QPushButton {"
                    "  background-color: %1;"
                    "  border: 1px solid %2;"
                    "  border-radius: 3px;"
                    "}")
                .arg(newColor.name())
                .arg(NMStyleManager::instance().palette().borderDark.name()));

        emit propertyValueChanged(propertyName, newColor.name());
      }
    });

    editor = colorButton;
    break;
  }

  case NMPropertyType::Asset: {
    auto* assetButton = new NMAssetButton(currentValue.isEmpty() ? "(Select Asset)" : currentValue);
    assetButton->setProperty("propertyName", propertyName);
    assetButton->setIconSize(QSize(22, 22));
    assetButton->setStyleSheet(QString("QPushButton {"
                                       "  background-color: %1;"
                                       "  color: %2;"
                                       "  border: 1px solid %3;"
                                       "  border-radius: 3px;"
                                       "  padding: 4px;"
                                       "  text-align: left;"
                                       "}"
                                       "QPushButton:hover {"
                                       "  border-color: %4;"
                                       "}")
                                   .arg(palette.bgDark.name())
                                   .arg(palette.textPrimary.name())
                                   .arg(palette.borderDark.name())
                                   .arg(palette.accentPrimary.name()));

    auto updateAssetButton = [assetButton](const QString& value) {
      const QString display = value.isEmpty() ? "(Select Asset)" : QFileInfo(value).fileName();
      assetButton->setText(display);
      assetButton->setToolTip(value);
      assetButton->setIcon(QIcon());

      if (value.isEmpty()) {
        return;
      }

      QString absPath = value;
      if (ProjectManager::instance().hasOpenProject()) {
        QFileInfo info(value);
        if (!info.isAbsolute()) {
          absPath = QString::fromStdString(
              ProjectManager::instance().toAbsolutePath(value.toStdString()));
        }
      }

      QFileInfo info(absPath);
      const QString ext = info.suffix().toLower();
      const bool isImage =
          (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "gif");
      if (isImage && info.exists()) {
        QPixmap pix(absPath);
        if (!pix.isNull()) {
          assetButton->setIcon(
              QIcon(pix.scaled(22, 22, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        }
      }
    };

    auto applyAssetPath = [this, propertyName, assetButton,
                           updateAssetButton](const QString& path) {
      if (path.isEmpty()) {
        return;
      }

      auto& projectManager = ProjectManager::instance();
      QString value = path;
      if (projectManager.hasOpenProject()) {
        if (projectManager.isPathInProject(path.toStdString())) {
          value = QString::fromStdString(projectManager.toRelativePath(path.toStdString()));
        } else {
          const QString assetsRoot =
              QString::fromStdString(projectManager.getFolderPath(ProjectFolder::Assets));
          const QString extension = QFileInfo(path).suffix();
          AssetImportChoice choice = promptAssetImport(this, assetsRoot, extension);
          if (!choice.accepted) {
            return;
          }
          if (!choice.useExternal) {
            QDir targetDir(choice.targetDir);
            if (!targetDir.exists()) {
              targetDir.mkpath(".");
            }
            const QString destPath =
                generateUniquePath(targetDir.absolutePath(), QFileInfo(path).fileName());
            if (!QFile::copy(path, destPath)) {
              NMMessageDialog::showWarning(this, tr("Import Asset"),
                                           tr("Failed to import asset into the project."));
              return;
            }
            value = QString::fromStdString(projectManager.toRelativePath(destPath.toStdString()));

            // Notify Asset Browser that a new asset was imported
            QtEventBus::instance().publishAssetImported(destPath, choice.targetDir);
          }
        }
      }

      updateAssetButton(value);
      emit propertyValueChanged(propertyName, value);
    };

    updateAssetButton(currentValue);

    connect(assetButton, &QPushButton::clicked, this, [this, propertyName, applyAssetPath]() {
      auto& projectManager = ProjectManager::instance();
      QString startDir;
      if (projectManager.hasOpenProject()) {
        startDir = QString::fromStdString(projectManager.getFolderPath(ProjectFolder::Assets));
      } else {
        startDir = QDir::homePath();
      }

      QString filter = tr("Assets (*.png *.jpg *.jpeg *.bmp *.gif *.wav *.mp3 *.ogg "
                          "*.flac *.ttf *.otf *.nms *.nmscene *.json *.xml *.yaml *.yml)");
      if (propertyName.contains("script", Qt::CaseInsensitive)) {
        filter = tr("Scripts (*.nms)");
      }
      QString path = NMFileDialog::getOpenFileName(this, tr("Select Asset"), startDir, filter);
      applyAssetPath(path);
    });

    connect(assetButton, &NMAssetButton::assetDropped, this,
            [applyAssetPath](const QString& path) { applyAssetPath(path); });

    editor = assetButton;
    break;
  }

  case NMPropertyType::Vector2: {
    // Create container widget for vector components
    auto* vectorWidget = new QWidget();
    vectorWidget->setProperty("propertyName", propertyName);
    auto* vectorLayout = new QHBoxLayout(vectorWidget);
    vectorLayout->setContentsMargins(0, 0, 0, 0);
    vectorLayout->setSpacing(4);

    // Parse current value (format: "x,y")
    QStringList components = currentValue.split(',');
    double x = components.size() > 0 ? components[0].toDouble() : 0.0;
    double y = components.size() > 1 ? components[1].toDouble() : 0.0;

    // Create spinboxes for X and Y
    auto createVectorSpinBox = [&palette](const QString& componentLabel,
                                          double value) -> QDoubleSpinBox* {
      auto* spinBox = new QDoubleSpinBox();
      spinBox->setPrefix(componentLabel + ": ");
      spinBox->setRange(-999999.0, 999999.0);
      spinBox->setDecimals(3);
      spinBox->setValue(value);
      spinBox->setStyleSheet(QString("QDoubleSpinBox {"
                                     "  background-color: %1;"
                                     "  color: %2;"
                                     "  border: 1px solid %3;"
                                     "  border-radius: 3px;"
                                     "  padding: 4px;"
                                     "}")
                                 .arg(palette.bgDark.name())
                                 .arg(palette.textPrimary.name())
                                 .arg(palette.borderDark.name()));
      return spinBox;
    };

    auto* xSpinBox = createVectorSpinBox("X", x);
    auto* ySpinBox = createVectorSpinBox("Y", y);

    vectorLayout->addWidget(xSpinBox);
    vectorLayout->addWidget(ySpinBox);

    // Use debounce timer to prevent spamming property updates
    auto* debounceTimer = new QTimer(vectorWidget);
    debounceTimer->setSingleShot(true);
    debounceTimer->setInterval(150);

    auto emitVectorChange = [this, propertyName, xSpinBox, ySpinBox]() {
      QString value =
          QString("%1,%2").arg(xSpinBox->value(), 0, 'f', 3).arg(ySpinBox->value(), 0, 'f', 3);
      emit propertyValueChanged(propertyName, value);
    };

    connect(debounceTimer, &QTimer::timeout, this, emitVectorChange);

    connect(xSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [debounceTimer]() { debounceTimer->start(); });
    connect(ySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [debounceTimer]() { debounceTimer->start(); });

    editor = vectorWidget;
    break;
  }

  case NMPropertyType::Vector3: {
    // Create container widget for vector components
    auto* vectorWidget = new QWidget();
    vectorWidget->setProperty("propertyName", propertyName);
    auto* vectorLayout = new QHBoxLayout(vectorWidget);
    vectorLayout->setContentsMargins(0, 0, 0, 0);
    vectorLayout->setSpacing(4);

    // Parse current value (format: "x,y,z")
    QStringList components = currentValue.split(',');
    double x = components.size() > 0 ? components[0].toDouble() : 0.0;
    double y = components.size() > 1 ? components[1].toDouble() : 0.0;
    double z = components.size() > 2 ? components[2].toDouble() : 0.0;

    // Create spinboxes for X, Y, and Z
    auto createVectorSpinBox = [&palette](const QString& componentLabel,
                                          double value) -> QDoubleSpinBox* {
      auto* spinBox = new QDoubleSpinBox();
      spinBox->setPrefix(componentLabel + ": ");
      spinBox->setRange(-999999.0, 999999.0);
      spinBox->setDecimals(3);
      spinBox->setValue(value);
      spinBox->setStyleSheet(QString("QDoubleSpinBox {"
                                     "  background-color: %1;"
                                     "  color: %2;"
                                     "  border: 1px solid %3;"
                                     "  border-radius: 3px;"
                                     "  padding: 4px;"
                                     "}")
                                 .arg(palette.bgDark.name())
                                 .arg(palette.textPrimary.name())
                                 .arg(palette.borderDark.name()));
      return spinBox;
    };

    auto* xSpinBox = createVectorSpinBox("X", x);
    auto* ySpinBox = createVectorSpinBox("Y", y);
    auto* zSpinBox = createVectorSpinBox("Z", z);

    vectorLayout->addWidget(xSpinBox);
    vectorLayout->addWidget(ySpinBox);
    vectorLayout->addWidget(zSpinBox);

    // Use debounce timer to prevent spamming property updates
    auto* debounceTimer = new QTimer(vectorWidget);
    debounceTimer->setSingleShot(true);
    debounceTimer->setInterval(150);

    auto emitVectorChange = [this, propertyName, xSpinBox, ySpinBox, zSpinBox]() {
      QString value = QString("%1,%2,%3")
                          .arg(xSpinBox->value(), 0, 'f', 3)
                          .arg(ySpinBox->value(), 0, 'f', 3)
                          .arg(zSpinBox->value(), 0, 'f', 3);
      emit propertyValueChanged(propertyName, value);
    };

    connect(debounceTimer, &QTimer::timeout, this, emitVectorChange);

    connect(xSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [debounceTimer]() { debounceTimer->start(); });
    connect(ySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [debounceTimer]() { debounceTimer->start(); });
    connect(zSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [debounceTimer]() { debounceTimer->start(); });

    editor = vectorWidget;
    break;
  }

  case NMPropertyType::Curve: {
    auto* curveButton = new QPushButton(tr("Edit Curve..."));
    curveButton->setProperty("propertyName", propertyName);
    curveButton->setProperty("curveId", currentValue);
    curveButton->setStyleSheet(QString("QPushButton {"
                                       "  background-color: %1;"
                                       "  color: %2;"
                                       "  border: 1px solid %3;"
                                       "  border-radius: 3px;"
                                       "  padding: 4px;"
                                       "}"
                                       "QPushButton:hover {"
                                       "  border-color: %4;"
                                       "}")
                                   .arg(palette.bgDark.name())
                                   .arg(palette.textPrimary.name())
                                   .arg(palette.borderDark.name())
                                   .arg(palette.accentPrimary.name()));

    connect(curveButton, &QPushButton::clicked, this, [this, propertyName, curveButton]() {
      const QString curveId = curveButton->property("curveId").toString();
      // Emit signal to request curve editor opening
      emit propertyValueChanged(propertyName + ":openCurveEditor", curveId);
    });

    editor = curveButton;
    break;
  }
  }

  if (editor) {
    addProperty(label, editor);
  }
  return editor;
}

QWidget* NMPropertyGroup::addEditableProperty(const QString& name, NMPropertyType type,
                                              const QString& currentValue,
                                              const QStringList& enumValues) {
  return addEditableProperty(name, name, type, currentValue, enumValues);
}

void NMPropertyGroup::onPropertyEdited() {
  QObject* sender = QObject::sender();
  if (!sender)
    return;

  QString propertyName = sender->property("propertyName").toString();
  QString newValue;

  if (auto* lineEdit = qobject_cast<QLineEdit*>(sender)) {
    newValue = lineEdit->text();
  } else if (auto* spinBox = qobject_cast<QSpinBox*>(sender)) {
    newValue = QString::number(spinBox->value());
  } else if (auto* doubleSpinBox = qobject_cast<QDoubleSpinBox*>(sender)) {
    newValue = QString::number(doubleSpinBox->value(), 'f', 3);
  } else if (auto* checkBox = qobject_cast<QCheckBox*>(sender)) {
    newValue = checkBox->isChecked() ? "true" : "false";
  } else if (auto* comboBox = qobject_cast<QComboBox*>(sender)) {
    newValue = comboBox->currentText();
  } else if (auto* textEdit = qobject_cast<QPlainTextEdit*>(sender)) {
    newValue = textEdit->toPlainText();
  }

  if (!propertyName.isEmpty()) {
    emit propertyValueChanged(propertyName, newValue);
  }
}

// ============================================================================

void NMPropertyGroup::addResetButton(const QString& propertyName, const QVariant& defaultValue) {
  // Create a reset button widget
  auto* resetButton = new QPushButton(tr("Reset"));
  resetButton->setProperty("propertyName", propertyName);
  resetButton->setProperty("defaultValue", defaultValue);
  resetButton->setMaximumWidth(60);
  resetButton->setToolTip(
      tr("Reset %1 to default value: %2").arg(propertyName).arg(defaultValue.toString()));

  const auto& palette = NMStyleManager::instance().palette();
  resetButton->setStyleSheet(QString("QPushButton {"
                                     "  background-color: %1;"
                                     "  color: %2;"
                                     "  border: 1px solid %3;"
                                     "  border-radius: 3px;"
                                     "  padding: 4px 8px;"
                                     "}"
                                     "QPushButton:hover {"
                                     "  border-color: %4;"
                                     "}")
                                 .arg(palette.bgDark.name())
                                 .arg(palette.textPrimary.name())
                                 .arg(palette.borderDark.name())
                                 .arg(palette.accentPrimary.name()));

  connect(resetButton, &QPushButton::clicked, this, [this, propertyName, defaultValue]() {
    emit propertyValueChanged(propertyName, defaultValue.toString());
  });

  // Add to the content layout
  auto* row = new QWidget(m_content);
  auto* rowLayout = new QHBoxLayout(row);
  rowLayout->setContentsMargins(0, 0, 0, 0);
  rowLayout->setSpacing(8);
  rowLayout->addStretch();
  rowLayout->addWidget(resetButton);

  m_contentLayout->addWidget(row);
}

QWidget* NMPropertyGroup::addEditablePropertyWithMetadata(const QString& propertyName,
                                                          NMPropertyType type,
                                                          const QString& currentValue,
                                                          const NMPropertyMetadata& metadata) {
  QWidget* editor = nullptr;
  const auto& palette = NMStyleManager::instance().palette();

  // Determine the display label from metadata or property name
  QString label = metadata.displayName.isEmpty() ? propertyName : metadata.displayName;

  switch (type) {
  case NMPropertyType::Float: {
    auto* doubleSpinBox = new QDoubleSpinBox();
    doubleSpinBox->setProperty("propertyName", propertyName);

    // Apply metadata constraints
    double minVal = metadata.minValue.isValid() ? metadata.minValue.toDouble() : -999999.0;
    double maxVal = metadata.maxValue.isValid() ? metadata.maxValue.toDouble() : 999999.0;
    doubleSpinBox->setRange(minVal, maxVal);

    // Default decimals to 2 unless specified
    doubleSpinBox->setDecimals(2);

    // Set single step from metadata or default to 0.1
    doubleSpinBox->setSingleStep(0.1);

    doubleSpinBox->setValue(currentValue.toDouble());

    if (!metadata.tooltip.isEmpty()) {
      doubleSpinBox->setToolTip(metadata.tooltip);
    }

    doubleSpinBox->setStyleSheet(QString("QDoubleSpinBox {"
                                         "  background-color: %1;"
                                         "  color: %2;"
                                         "  border: 1px solid %3;"
                                         "  border-radius: 3px;"
                                         "  padding: 4px;"
                                         "}")
                                     .arg(palette.bgDark.name())
                                     .arg(palette.textPrimary.name())
                                     .arg(palette.borderDark.name()));

    connect(doubleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            &NMPropertyGroup::onPropertyEdited);
    editor = doubleSpinBox;
    break;
  }

  default:
    // Fall back to the standard method for other types
    return addEditableProperty(propertyName, label, type, currentValue);
  }

  if (editor) {
    addProperty(label, editor);
  }
  return editor;
}

} // namespace NovelMind::editor::qt

#include "nm_inspector_panel_group.moc"
