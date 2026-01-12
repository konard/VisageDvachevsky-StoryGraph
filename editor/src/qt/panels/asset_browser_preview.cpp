#include "NovelMind/editor/qt/panels/asset_browser_preview.hpp"
#include "NovelMind/editor/qt/panels/asset_browser_categorizer.hpp"

#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QPixmap>
#include <QVBoxLayout>

namespace NovelMind::editor::qt {

namespace {
constexpr int kPreviewHeight = 140;
}

NMAssetPreviewFrame::NMAssetPreviewFrame(QWidget *parent) : QFrame(parent) {
  setObjectName("AssetPreviewFrame");
  setFrameShape(QFrame::StyledPanel);
  setupUi();
}

void NMAssetPreviewFrame::setupUi() {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->setSpacing(3);

  m_previewImage = new QLabel(this);
  m_previewImage->setAlignment(Qt::AlignCenter);
  m_previewImage->setMinimumHeight(kPreviewHeight);
  m_previewImage->setText(QObject::tr("No preview"));

  m_previewName = new QLabel(this);
  m_previewName->setWordWrap(true);

  m_previewMeta = new QLabel(this);
  m_previewMeta->setWordWrap(true);

  layout->addWidget(m_previewImage);
  layout->addWidget(m_previewName);
  layout->addWidget(m_previewMeta);
}

void NMAssetPreviewFrame::updatePreview(const QString &path) {
  m_previewPath = path;

  if (path.isEmpty()) {
    clearPreview();
    return;
  }

  QFileInfo info(path);
  if (!info.exists() || !info.isFile()) {
    clearPreview();
    return;
  }

  m_previewName->setText(info.fileName());
  const QString sizeText =
      QObject::tr("%1 KB").arg(QString::number((info.size() + 1023) / 1024));

  if (isImageExtension(info.suffix())) {
    QImageReader reader(info.absoluteFilePath());
    reader.setAutoTransform(true);
    QSize imgSize = reader.size();
    QImage image = reader.read();
    if (!image.isNull()) {
      QPixmap pix = QPixmap::fromImage(image);
      QSize target = m_previewImage->size();
      QPixmap scaled =
          pix.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
      m_previewImage->setPixmap(scaled);
      m_previewImage->setText(QString());
      m_previewMeta->setText(QObject::tr("%1 x %2 | %3")
                                 .arg(imgSize.width())
                                 .arg(imgSize.height())
                                 .arg(sizeText));
      return;
    }
  }

  m_previewImage->setPixmap(QPixmap());
  m_previewImage->setText(QObject::tr("No preview"));
  m_previewMeta->setText(sizeText);
}

void NMAssetPreviewFrame::clearPreview() {
  m_previewPath.clear();

  if (m_previewImage) {
    m_previewImage->setPixmap(QPixmap());
    m_previewImage->setText(QObject::tr("No preview"));
  }

  if (m_previewName) {
    m_previewName->setText(QString());
  }

  if (m_previewMeta) {
    m_previewMeta->setText(QString());
  }
}

} // namespace NovelMind::editor::qt
