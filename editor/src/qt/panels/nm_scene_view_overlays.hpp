#pragma once

#include "NovelMind/core/logger.hpp"
#include "NovelMind/editor/editor_runtime_host.hpp"
#include "NovelMind/editor/qt/nm_style_manager.hpp"
#include "NovelMind/renderer/camera.hpp"
#include "NovelMind/renderer/font.hpp"
#include "NovelMind/renderer/text_layout.hpp"

#include <QColor>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QHash>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPixmap>
#include <QPointF>
#include <QPushButton>
#include <QSizePolicy>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace NovelMind::editor::qt {

// ============================================================================
// NMPlayPreviewOverlay
// ============================================================================

class NMPlayPreviewOverlay : public QWidget {
  Q_OBJECT

public:
  explicit NMPlayPreviewOverlay(QWidget *parent = nullptr)
      : QWidget(parent), m_typeTimer(new QTimer(this)) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::StrongFocus);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 24, 24, 18);
    layout->addStretch();

    // Preview mode indicator badge
    m_previewBadge = new QLabel(this);
    m_previewBadge->setObjectName("PreviewBadge");
    m_previewBadge->setText(tr("PREVIEW"));
    m_previewBadge->setAlignment(Qt::AlignCenter);
    m_previewBadge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_previewBadge->setMinimumSize(90, 28);
    m_previewBadge->hide(); // Hidden by default
    layout->addWidget(m_previewBadge, 0, Qt::AlignHCenter);

    m_dialogueBox = new QFrame(this);
    m_dialogueBox->setObjectName("DialogueBox");
    m_dialogueBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_dialogueBox->setMinimumHeight(150);
    auto *dialogueLayout = new QVBoxLayout(m_dialogueBox);
    dialogueLayout->setContentsMargins(22, 18, 22, 20);
    dialogueLayout->setSpacing(8);

    m_namePlate = new QFrame(m_dialogueBox);
    m_namePlate->setObjectName("NamePlate");
    auto *nameLayout = new QHBoxLayout(m_namePlate);
    nameLayout->setContentsMargins(10, 4, 10, 4);
    nameLayout->setSpacing(6);

    m_nameLabel = new QLabel(m_namePlate);
    m_nameLabel->setObjectName("NameLabel");
    m_nameLabel->setText("Narrator");
    nameLayout->addWidget(m_nameLabel);
    nameLayout->addStretch();

    m_textLabel = new QLabel(m_dialogueBox);
    m_textLabel->setObjectName("TextLabel");
    m_textLabel->setWordWrap(true);
    m_textLabel->setTextInteractionFlags(Qt::NoTextInteraction);
    m_textLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    QFont nameFont("PT Serif", 12, QFont::Bold);
    nameFont.setStyleHint(QFont::Serif);
    m_nameLabel->setFont(nameFont);
    QFont textFont("PT Serif", 13);
    textFont.setStyleHint(QFont::Serif);
    m_textLabel->setFont(textFont);

    dialogueLayout->addWidget(m_namePlate);
    dialogueLayout->addWidget(m_textLabel);

    m_choicesBox = new QWidget(this);
    m_choicesBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_choicesLayout = new QVBoxLayout(m_choicesBox);
    m_choicesLayout->setContentsMargins(40, 12, 40, 0);
    m_choicesLayout->setSpacing(10);
    layout->addWidget(m_choicesBox);
    layout->addWidget(m_dialogueBox);

    m_dialogueBox->hide();
    m_choicesBox->hide();

    const auto &palette = NMStyleManager::instance().palette();
    setStyleSheet(
        QString("QFrame#DialogueBox {"
                "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                "stop:0 rgba(12, 12, 16, 235), stop:1 rgba(6, 6, 8, 235));"
                "  border: 1px solid %1;"
                "  border-radius: 14px;"
                "}"
                "QFrame#NamePlate {"
                "  background-color: rgba(20, 20, 24, 220);"
                "  border: 1px solid %1;"
                "  border-radius: 10px;"
                "}"
                "QLabel#NameLabel {"
                "  color: %2;"
                "  font-weight: bold;"
                "  letter-spacing: 0.4px;"
                "}"
                "QLabel#TextLabel {"
                "  color: %3;"
                "}"
                "QLabel#PreviewBadge {"
                "  background-color: rgba(255, 165, 0, 200);"
                "  color: rgba(20, 20, 20, 255);"
                "  border: 1px solid rgba(255, 200, 100, 255);"
                "  border-radius: 6px;"
                "  padding: 4px 12px;"
                "  font-weight: bold;"
                "  font-size: 11px;"
                "  letter-spacing: 0.8px;"
                "}"
                "QPushButton#ChoiceButton {"
                "  background-color: rgba(18, 18, 24, 225);"
                "  color: %3;"
                "  border: 1px solid %1;"
                "  border-radius: 10px;"
                "  padding: 12px 16px;"
                "  text-align: left;"
                "}"
                "QPushButton#ChoiceButton:hover {"
                "  border-color: %2;"
                "  background-color: rgba(28, 28, 36, 230);"
                "}")
            .arg(palette.borderLight.name())
            .arg(palette.accentPrimary.name())
            .arg(palette.textPrimary.name()));

    auto *dialogueShadow = new QGraphicsDropShadowEffect(this);
    dialogueShadow->setBlurRadius(18);
    dialogueShadow->setOffset(0, 4);
    dialogueShadow->setColor(QColor(0, 0, 0, 180));
    m_dialogueBox->setGraphicsEffect(dialogueShadow);

    connect(m_typeTimer, &QTimer::timeout, this, [this]() {
      if (m_fullText.isEmpty()) {
        m_typeTimer->stop();
        return;
      }
      const int textLength = static_cast<int>(std::min<qsizetype>(
          m_fullText.size(), std::numeric_limits<int>::max()));
      m_typeIndex = std::min(m_typeIndex + 1, textLength);
      m_textLabel->setText(m_fullText.left(m_typeIndex));
      if (m_typeIndex >= textLength) {
        m_typeTimer->stop();
      }
    });

    m_dialogueBox->installEventFilter(this);
    m_namePlate->installEventFilter(this);
    m_nameLabel->installEventFilter(this);
    m_textLabel->installEventFilter(this);

    hide();
  }

  void setDialogue(const QString &speaker, const QString &text) {
    m_nameLabel->setText(speaker.isEmpty() ? "Narrator" : speaker);
    m_namePlate->setVisible(!speaker.isEmpty());
    m_fullText = text;
    m_typeIndex = 0;
    m_textLabel->clear();
    m_dialogueBox->setVisible(!text.isEmpty());
    if (!text.isEmpty()) {
      m_typeTimer->start(m_typeIntervalMs);
    } else {
      m_typeTimer->stop();
    }
  }

  void setDialogueImmediate(const QString &speaker, const QString &text) {
    m_nameLabel->setText(speaker.isEmpty() ? "Narrator" : speaker);
    m_namePlate->setVisible(!speaker.isEmpty());
    m_fullText = text;
    m_typeTimer->stop();
    m_textLabel->setText(text);
    m_dialogueBox->setVisible(!text.isEmpty());
  }

  void clearDialogue() { setDialogue(QString(), QString()); }

  void setChoices(const QStringList &choices) {
    clearChoices();
    if (choices.isEmpty()) {
      return;
    }
    for (int i = 0; i < choices.size(); ++i) {
      auto *button = new QPushButton(choices.at(i), m_choicesBox);
      button->setObjectName("ChoiceButton");
      button->setMinimumHeight(38);
      button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
      connect(button, &QPushButton::clicked, this,
              [this, i]() { emit choiceSelected(i); });
      m_choicesLayout->addWidget(button);
    }
    m_choicesBox->setVisible(true);
  }

  void clearChoices() {
    QLayoutItem *item;
    while ((item = m_choicesLayout->takeAt(0)) != nullptr) {
      delete item->widget();
      delete item;
    }
    m_choicesBox->setVisible(false);
  }

  void setTypewriterSpeed(int charsPerSecond) {
    if (charsPerSecond <= 0) {
      m_typeIntervalMs = 30;
      return;
    }
    m_typeIntervalMs = std::max(10, 1000 / charsPerSecond);
  }

  void setInteractionEnabled(bool enabled) {
    setAttribute(Qt::WA_TransparentForMouseEvents, !enabled);
    setFocusPolicy(enabled ? Qt::StrongFocus : Qt::NoFocus);
  }

  void setPreviewMode(bool isPreview) {
    if (m_previewBadge) {
      m_previewBadge->setVisible(isPreview);
    }
  }

signals:
  void choiceSelected(int index);
  void advanceRequested();

protected:
  void mousePressEvent(QMouseEvent *event) override {
    if (event->button() == Qt::LeftButton) {
      emit advanceRequested();
    }
    QWidget::mousePressEvent(event);
  }

  void keyPressEvent(QKeyEvent *event) override {
    if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return ||
        event->key() == Qt::Key_Enter) {
      emit advanceRequested();
      event->accept();
      return;
    }
    QWidget::keyPressEvent(event);
  }

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (event->type() == QEvent::MouseButtonPress) {
      if (m_choicesBox && m_choicesBox->isVisible()) {
        return QWidget::eventFilter(watched, event);
      }
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() == Qt::LeftButton) {
        emit advanceRequested();
        return true;
      }
    }
    return QWidget::eventFilter(watched, event);
  }

private:
  QLabel *m_previewBadge = nullptr;
  QFrame *m_dialogueBox = nullptr;
  QFrame *m_namePlate = nullptr;
  QLabel *m_nameLabel = nullptr;
  QLabel *m_textLabel = nullptr;
  QWidget *m_choicesBox = nullptr;
  QVBoxLayout *m_choicesLayout = nullptr;
  QTimer *m_typeTimer = nullptr;
  QString m_fullText;
  int m_typeIndex = 0;
  int m_typeIntervalMs = 30;
};

// ============================================================================
// NMSceneGLViewport - OpenGL runtime preview (offscreen)
// ============================================================================

class NMSceneGLViewport : public QOpenGLWidget, protected QOpenGLFunctions {
  Q_OBJECT

public:
  explicit NMSceneGLViewport(QWidget *parent = nullptr)
      : QOpenGLWidget(parent) {
    setAutoFillBackground(false);
  }

  ~NMSceneGLViewport() override {
    auto *ctx = context();
    if (!ctx || !ctx->isValid()) {
      m_textureCache.clear();
      return;
    }

    makeCurrent();
    if (QOpenGLContext::currentContext() != ctx) {
      m_textureCache.clear();
      return;
    }

    for (const auto &entry : m_textureCache) {
      if (entry.id != 0) {
        GLuint tex = entry.id;
        glDeleteTextures(1, &tex);
      }
    }
    doneCurrent();
    m_textureCache.clear();
  }

  void setSnapshot(const ::NovelMind::editor::SceneSnapshot &snapshot,
                   const QString &assetsRoot) {
    m_snapshot = snapshot;
    m_assetsRoot = assetsRoot;
    if (snapshot.camera.valid) {
      m_camera.setPosition(snapshot.camera.x, snapshot.camera.y);
      m_camera.setZoom(snapshot.camera.zoom);
      m_camera.setRotation(snapshot.camera.rotation);
    }
    update();
  }

  void setRenderDialogue(bool enabled) { m_renderDialogue = enabled; }
  void setRenderChoice(bool enabled) { m_renderChoice = enabled; }

  [[nodiscard]] QString fontAtlasStatus() const { return m_fontAtlasStatus; }
  [[nodiscard]] bool hasFontAtlas() const {
    return m_fontAtlas && m_fontAtlas->isValid();
  }

  void setViewCamera(const QPointF &center, qreal zoom) {
    m_camera.setViewportSize(static_cast<f32>(width()),
                             static_cast<f32>(height()));
    const f32 baseX = m_snapshot.camera.valid ? m_snapshot.camera.x : 0.0f;
    const f32 baseY = m_snapshot.camera.valid ? m_snapshot.camera.y : 0.0f;
    const f32 baseZoom =
        m_snapshot.camera.valid ? m_snapshot.camera.zoom : 1.0f;
    const f32 centerX = static_cast<f32>(center.x());
    const f32 centerY = static_cast<f32>(center.y());
    const f32 offsetX = centerX - baseX;
    const f32 offsetY = centerY - baseY;
    const f32 zoomFactor = static_cast<f32>(zoom);
    m_camera.setPosition(baseX + offsetX, baseY + offsetY);
    m_camera.setZoom(baseZoom * zoomFactor);
    if (m_snapshot.camera.valid) {
      m_camera.setRotation(m_snapshot.camera.rotation);
    }
  }

protected:
  void initializeGL() override {
    initializeOpenGLFunctions();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glClearColor(0.05f, 0.05f, 0.07f, 1.0f);
    ensureFontAtlas();
  }

  void resizeGL(int w, int h) override {
    m_camera.setViewportSize(static_cast<f32>(w), static_cast<f32>(h));
    QOpenGLWidget::resizeGL(w, h);
  }

  void paintGL() override {
    if (!context() || !context()->isValid()) {
      return;
    }
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_snapshot.objects.empty()) {
      drawEmptyBackdrop();
      return;
    }

    setupCameraTransform();
    renderObjects();
    if (m_renderDialogue) {
      renderDialogue();
    }
    if (m_renderChoice) {
      renderChoice();
    }
  }

private:
  struct GLTexture {
    GLuint id = 0;
    i32 width = 0;
    i32 height = 0;
  };

  GLTexture uploadTexture(const QImage &img, const QString &cacheKey) {
    QImage glImg = img.convertToFormat(QImage::Format_RGBA8888);
    GLTexture tex;
    glGenTextures(1, &tex.id);
    glBindTexture(GL_TEXTURE_2D, tex.id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glImg.width(), glImg.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, glImg.constBits());

    tex.width = glImg.width();
    tex.height = glImg.height();
    m_textureCache[cacheKey] = tex;
    return tex;
  }

  GLTexture placeholderTexture(const QString &label,
                               scene::SceneObjectType type) {
    QSize sz = (type == scene::SceneObjectType::Background) ? QSize(1280, 720)
                                                            : QSize(400, 600);
    QImage img(sz, QImage::Format_ARGB32_Premultiplied);
    if (img.isNull()) {
      QImage fallback(QSize(1, 1), QImage::Format_ARGB32_Premultiplied);
      fallback.fill(Qt::transparent);
      return uploadTexture(fallback, label);
    }
    QColor fill = (type == scene::SceneObjectType::Background)
                      ? QColor(40, 50, 70)
                      : QColor(70, 80, 95);
    img.fill(fill);
    QPainter p(&img);
    if (!p.isActive()) {
      QImage fallback(QSize(1, 1), QImage::Format_ARGB32_Premultiplied);
      fallback.fill(Qt::transparent);
      return uploadTexture(fallback, label);
    }
    p.setPen(QColor(180, 200, 255));
    p.drawRect(QRect(QPoint(0, 0), sz - QSize(1, 1)));
    p.drawText(QRect(QPoint(0, 0), sz), Qt::AlignCenter, label);
    p.end();
    return uploadTexture(QPixmap::fromImage(img).toImage(), label);
  }

  GLTexture resolveTexture(const QString &hint, scene::SceneObjectType type) {
    if (m_textureCache.contains(hint)) {
      return m_textureCache.value(hint);
    }

    QString baseName = hint;
    if (baseName.isEmpty()) {
      return placeholderTexture("missing", type);
    }

    QStringList exts = {"", ".png", ".jpg", ".jpeg"};
    QStringList prefixes = {m_assetsRoot + "/", m_assetsRoot + "/Images/",
                            m_assetsRoot + "/images/", QString()};
    for (const auto &prefix : prefixes) {
      for (const auto &ext : exts) {
        const QString path = prefix + baseName + ext;
        if (!path.isEmpty() && QFileInfo::exists(path)) {
          QImage img(path);
          if (!img.isNull()) {
            return uploadTexture(img, hint);
          }
        }
      }
    }

    return placeholderTexture(baseName, type);
  }

  void setupCameraTransform() {
    glViewport(0, 0, width(), height());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, static_cast<GLdouble>(width()),
            static_cast<GLdouble>(height()), 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    const auto viewTransform = m_camera.getViewTransform();
    glTranslatef(static_cast<GLfloat>(width()) * 0.5f,
                 static_cast<GLfloat>(height()) * 0.5f, 0.0f);
    glScalef(viewTransform.scaleX, viewTransform.scaleY, 1.0f);
    glRotatef(-viewTransform.rotation, 0.0f, 0.0f, 1.0f);
    glTranslatef(-viewTransform.x, -viewTransform.y, 0.0f);
  }

  static QString textureHintFromState(const scene::SceneObjectState &state) {
    const char *keys[] = {"textureId", "texture", "image", "sprite",
                          "background"};
    for (auto *key : keys) {
      auto it = state.properties.find(key);
      if (it != state.properties.end()) {
        return QString::fromStdString(it->second);
      }
    }
    return QString::fromStdString(state.id);
  }

  void renderObjects() {
    std::vector<const scene::SceneObjectState *> sorted;
    sorted.reserve(m_snapshot.objects.size());
    for (const auto &obj : m_snapshot.objects) {
      sorted.push_back(&obj);
    }
    std::sort(
        sorted.begin(), sorted.end(),
        [](const scene::SceneObjectState *a, const scene::SceneObjectState *b) {
          return a->zOrder < b->zOrder;
        });

    glEnable(GL_TEXTURE_2D);

    for (const auto *state : sorted) {
      if (!state->visible) {
        continue;
      }

      GLTexture tex = resolveTexture(textureHintFromState(*state), state->type);

      const f32 drawW =
          (state->width > 0.0f) ? state->width : static_cast<f32>(tex.width);
      const f32 drawH =
          (state->height > 0.0f) ? state->height : static_cast<f32>(tex.height);

      const f32 anchorX = drawW * 0.5f;
      const f32 anchorY = drawH * 0.5f;

      glPushMatrix();
      glTranslatef(state->x, state->y, 0.0f);
      glTranslatef(anchorX, anchorY, 0.0f);
      glRotatef(state->rotation, 0.0f, 0.0f, 1.0f);
      glScalef(state->scaleX, state->scaleY, 1.0f);
      glTranslatef(-anchorX, -anchorY, 0.0f);

      const f32 alpha = state->alpha;
      glColor4f(1.0f, 1.0f, 1.0f, alpha);

      if (tex.id != 0) {
        glBindTexture(GL_TEXTURE_2D, tex.id);
        glBegin(GL_QUADS);
        glTexCoord2f(0.0f, 0.0f);
        glVertex2f(0.0f, 0.0f);

        glTexCoord2f(1.0f, 0.0f);
        glVertex2f(drawW, 0.0f);

        glTexCoord2f(1.0f, 1.0f);
        glVertex2f(drawW, drawH);

        glTexCoord2f(0.0f, 1.0f);
        glVertex2f(0.0f, drawH);
        glEnd();
      } else {
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_QUADS);
        glVertex2f(0.0f, 0.0f);
        glVertex2f(drawW, 0.0f);
        glVertex2f(drawW, drawH);
        glVertex2f(0.0f, drawH);
        glEnd();
        glEnable(GL_TEXTURE_2D);
      }

      glPopMatrix();
    }
  }

  void renderDialogue() {
    if (!m_snapshot.dialogueVisible) {
      return;
    }
    if (!m_fontAtlas || !m_fontAtlas->isValid()) {
      return;
    }

    renderer::TextStyle style;
    style.size = 24.0f;
    style.color = renderer::Color::white();

    m_textLayout.setFontAtlas(m_fontAtlas);
    m_textLayout.setDefaultStyle(style);
    m_textLayout.setMaxWidth(static_cast<f32>(width()) * 0.8f);

    renderer::TextLayout layout = m_textLayout.layout(m_snapshot.dialogueText);

    const auto &atlasTex = m_fontAtlas->getAtlasTexture();
    auto atlasId = static_cast<GLuint>(
        reinterpret_cast<uintptr_t>(atlasTex.getNativeHandle()));
    if (atlasId == 0) {
      return;
    }

    glBindTexture(GL_TEXTURE_2D, atlasId);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    f32 originX = static_cast<f32>(width()) * 0.1f;
    f32 originY = static_cast<f32>(height()) - (layout.totalHeight + 40.0f);

    f32 penY = originY;
    for (const auto &line : layout.lines) {
      f32 penX = originX;
      const f32 baseline = penY + line.height * 0.8f;
      for (const auto &segment : line.segments) {
        if (segment.isCommand()) {
          continue;
        }
        for (char c : segment.text) {
          const auto *glyph =
              m_fontAtlas->getGlyph(static_cast<unsigned char>(c));
          if (!glyph) {
            continue;
          }

          f32 x0 = penX + glyph->bearingX;
          f32 y0 = baseline - glyph->bearingY;
          f32 x1 = x0 + glyph->width;
          f32 y1 = y0 + glyph->height;

          glBegin(GL_QUADS);
          glTexCoord2f(glyph->uv.x, glyph->uv.y);
          glVertex2f(x0, y0);

          glTexCoord2f(glyph->uv.x + glyph->uv.width, glyph->uv.y);
          glVertex2f(x1, y0);

          glTexCoord2f(glyph->uv.x + glyph->uv.width,
                       glyph->uv.y + glyph->uv.height);
          glVertex2f(x1, y1);

          glTexCoord2f(glyph->uv.x, glyph->uv.y + glyph->uv.height);
          glVertex2f(x0, y1);
          glEnd();

          penX += glyph->advanceX;
        }
      }
      penY += line.height;
    }
  }

  void renderChoice() {
    if (!m_snapshot.choiceMenuVisible) {
      return;
    }
    if (!m_fontAtlas || !m_fontAtlas->isValid()) {
      return;
    }
    if (m_snapshot.choiceOptions.empty()) {
      return;
    }

    renderer::TextStyle style;
    style.size = 22.0f;
    style.color = renderer::Color::white();

    m_textLayout.setFontAtlas(m_fontAtlas);
    m_textLayout.setDefaultStyle(style);

    const f32 choiceWidth = static_cast<f32>(width()) * 0.6f;
    const f32 choiceHeight = 40.0f;
    const f32 choiceSpacing = 12.0f;
    const f32 totalHeight = static_cast<f32>(m_snapshot.choiceOptions.size()) *
                                (choiceHeight + choiceSpacing) -
                            choiceSpacing;

    f32 originX = (static_cast<f32>(width()) - choiceWidth) * 0.5f;
    f32 originY = (static_cast<f32>(height()) - totalHeight) * 0.5f;

    glDisable(GL_TEXTURE_2D);

    for (size_t i = 0; i < m_snapshot.choiceOptions.size(); ++i) {
      const f32 y =
          originY + static_cast<f32>(i) * (choiceHeight + choiceSpacing);

      // Draw choice background
      renderer::Color bgColor =
          (static_cast<i32>(i) == m_snapshot.selectedChoice)
              ? renderer::Color(40, 50, 70, 230)
              : renderer::Color(20, 25, 35, 220);

      glColor4f(static_cast<f32>(bgColor.r) / 255.0f,
                static_cast<f32>(bgColor.g) / 255.0f,
                static_cast<f32>(bgColor.b) / 255.0f,
                static_cast<f32>(bgColor.a) / 255.0f);

      glBegin(GL_QUADS);
      glVertex2f(originX, y);
      glVertex2f(originX + choiceWidth, y);
      glVertex2f(originX + choiceWidth, y + choiceHeight);
      glVertex2f(originX, y + choiceHeight);
      glEnd();

      // Draw border
      renderer::Color borderColor =
          (static_cast<i32>(i) == m_snapshot.selectedChoice)
              ? renderer::Color(100, 140, 200, 255)
              : renderer::Color(60, 70, 90, 255);

      glColor4f(static_cast<f32>(borderColor.r) / 255.0f,
                static_cast<f32>(borderColor.g) / 255.0f,
                static_cast<f32>(borderColor.b) / 255.0f,
                static_cast<f32>(borderColor.a) / 255.0f);

      glLineWidth(2.0f);
      glBegin(GL_LINE_LOOP);
      glVertex2f(originX, y);
      glVertex2f(originX + choiceWidth, y);
      glVertex2f(originX + choiceWidth, y + choiceHeight);
      glVertex2f(originX, y + choiceHeight);
      glEnd();
    }

    glEnable(GL_TEXTURE_2D);

    // Render choice text
    const auto &atlasTex = m_fontAtlas->getAtlasTexture();
    auto atlasId = static_cast<GLuint>(
        reinterpret_cast<uintptr_t>(atlasTex.getNativeHandle()));
    if (atlasId == 0) {
      return;
    }

    glBindTexture(GL_TEXTURE_2D, atlasId);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    for (size_t i = 0; i < m_snapshot.choiceOptions.size(); ++i) {
      const f32 y =
          originY + static_cast<f32>(i) * (choiceHeight + choiceSpacing);
      const std::string &choiceText = m_snapshot.choiceOptions[i];

      f32 penX = originX + 16.0f;
      const f32 baseline = y + choiceHeight * 0.65f;

      for (char c : choiceText) {
        const auto *glyph =
            m_fontAtlas->getGlyph(static_cast<unsigned char>(c));
        if (!glyph) {
          continue;
        }

        f32 x0 = penX + glyph->bearingX;
        f32 y0 = baseline - glyph->bearingY;
        f32 x1 = x0 + glyph->width;
        f32 y1 = y0 + glyph->height;

        glBegin(GL_QUADS);
        glTexCoord2f(glyph->uv.x, glyph->uv.y);
        glVertex2f(x0, y0);

        glTexCoord2f(glyph->uv.x + glyph->uv.width, glyph->uv.y);
        glVertex2f(x1, y0);

        glTexCoord2f(glyph->uv.x + glyph->uv.width,
                     glyph->uv.y + glyph->uv.height);
        glVertex2f(x1, y1);

        glTexCoord2f(glyph->uv.x, glyph->uv.y + glyph->uv.height);
        glVertex2f(x0, y1);
        glEnd();

        penX += glyph->advanceX;
      }
    }
  }

  void drawEmptyBackdrop() {
    glDisable(GL_TEXTURE_2D);
    glColor4f(0.07f, 0.07f, 0.08f, 1.0f);
    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(static_cast<GLfloat>(width()), 0.0f);
    glVertex2f(static_cast<GLfloat>(width()), static_cast<GLfloat>(height()));
    glVertex2f(0.0f, static_cast<GLfloat>(height()));
    glEnd();
    glEnable(GL_TEXTURE_2D);
  }

  void ensureFontAtlas() {
    if (m_fontAtlas && m_fontAtlas->isValid()) {
      m_fontAtlasStatus.clear();
      return;
    }

    static const QStringList candidateFonts = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "C:/Windows/Fonts/arial.ttf"};

    std::vector<u8> fontData;
    for (const auto &path : candidateFonts) {
      QFile f(path);
      if (f.exists() && f.open(QIODevice::ReadOnly)) {
        auto bytes = f.readAll();
        fontData.assign(bytes.begin(), bytes.end());
        f.close();
        break;
      }
    }

    if (fontData.empty()) {
      m_fontAtlasStatus = "No system font found. Dialogue preview disabled.";
      NOVELMIND_LOG_WARN(
          "FontAtlas: no system font found, dialogue text preview disabled");
      return;
    }

    auto font = std::make_shared<renderer::Font>();
    auto loadRes = font->loadFromMemory(fontData, 24);
    if (loadRes.isError()) {
      m_fontAtlasStatus = "Failed to load font. Dialogue preview disabled.";
      NOVELMIND_LOG_WARN("FontAtlas: failed to load font from memory");
      return;
    }

    auto atlas = std::make_shared<renderer::FontAtlas>();
    auto res = atlas->build(*font, "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                   "abcdefghijklmnopqrstuvwxyz"
                                   "0123456789.,!?;:-_()[]{}<>/\\'\" ");
    if (res.isError()) {
      m_fontAtlasStatus = "Font atlas build failed. Dialogue preview disabled.";
      NOVELMIND_LOG_WARN(std::string("FontAtlas: build failed: ") +
                         res.error());
      return;
    }

    m_fontAtlas = atlas;
    m_textLayout.setFontAtlas(m_fontAtlas);
    m_fontAtlasStatus.clear();
  }

  ::NovelMind::editor::SceneSnapshot m_snapshot;
  QString m_assetsRoot;
  QHash<QString, GLTexture> m_textureCache;
  renderer::Camera2D m_camera;
  renderer::TextLayoutEngine m_textLayout;
  std::shared_ptr<renderer::FontAtlas> m_fontAtlas;
  QString m_fontAtlasStatus;
  bool m_renderDialogue = true;
  bool m_renderChoice = true;
};

// ============================================================================

} // namespace NovelMind::editor::qt
