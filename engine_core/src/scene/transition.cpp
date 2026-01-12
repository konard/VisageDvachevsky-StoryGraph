#include "NovelMind/scene/transition.hpp"
#include <algorithm>
#include <cmath>

namespace NovelMind::Scene {

// ============================================================================
// FadeTransition
// ============================================================================

FadeTransition::FadeTransition(const renderer::Color& fadeColor, bool fadeOut)
    : m_fadeColor(fadeColor), m_fadeOut(fadeOut), m_duration(1.0f), m_elapsed(0.0f),
      m_running(false), m_complete(false), m_onComplete(nullptr) {}

FadeTransition::~FadeTransition() = default;

void FadeTransition::start(f32 duration) {
  m_duration = duration;
  m_elapsed = 0.0f;
  m_running = true;
  m_complete = false;
}

void FadeTransition::update(f64 deltaTime) {
  if (!m_running || m_complete) {
    return;
  }

  m_elapsed += static_cast<f32>(deltaTime);

  if (m_elapsed >= m_duration) {
    m_elapsed = m_duration;
    m_running = false;
    m_complete = true;

    if (m_onComplete) {
      m_onComplete();
    }
  }
}

void FadeTransition::render(renderer::IRenderer& renderer) {
  if (!m_running && !m_complete) {
    return;
  }

  f32 progress = getProgress();

  // Smooth-step easing
  f32 t = progress * progress * (3.0f - 2.0f * progress);

  // Calculate alpha
  f32 alpha;
  if (m_fadeOut) {
    alpha = t; // 0 -> 1 (transparent to opaque)
  } else {
    alpha = 1.0f - t; // 1 -> 0 (opaque to transparent)
  }

  renderer::Color fadeColorWithAlpha = m_fadeColor;
  fadeColorWithAlpha.a = static_cast<u8>(alpha * 255.0f);

  renderer.setFade(alpha, fadeColorWithAlpha);
}

bool FadeTransition::isComplete() const {
  return m_complete;
}

f32 FadeTransition::getProgress() const {
  if (m_duration <= 0.0f) {
    return 1.0f;
  }
  return std::clamp(m_elapsed / m_duration, 0.0f, 1.0f);
}

void FadeTransition::setOnComplete(CompletionCallback callback) {
  m_onComplete = std::move(callback);
}

TransitionType FadeTransition::getType() const {
  return TransitionType::Fade;
}

void FadeTransition::setFadeColor(const renderer::Color& color) {
  m_fadeColor = color;
}

void FadeTransition::setFadeOut(bool fadeOut) {
  m_fadeOut = fadeOut;
}

// ============================================================================
// FadeThroughTransition
// ============================================================================

FadeThroughTransition::FadeThroughTransition(const renderer::Color& fadeColor)
    : m_fadeColor(fadeColor), m_duration(1.0f), m_elapsed(0.0f), m_running(false),
      m_complete(false), m_pastMidpoint(false), m_onComplete(nullptr), m_onMidpoint(nullptr) {}

FadeThroughTransition::~FadeThroughTransition() = default;

void FadeThroughTransition::start(f32 duration) {
  m_duration = duration;
  m_elapsed = 0.0f;
  m_running = true;
  m_complete = false;
  m_pastMidpoint = false;
}

void FadeThroughTransition::update(f64 deltaTime) {
  if (!m_running || m_complete) {
    return;
  }

  f32 prevElapsed = m_elapsed;
  m_elapsed += static_cast<f32>(deltaTime);

  // Check for midpoint
  f32 midpoint = m_duration * 0.5f;
  if (!m_pastMidpoint && prevElapsed < midpoint && m_elapsed >= midpoint) {
    m_pastMidpoint = true;
    if (m_onMidpoint) {
      m_onMidpoint();
    }
  }

  if (m_elapsed >= m_duration) {
    m_elapsed = m_duration;
    m_running = false;
    m_complete = true;

    if (m_onComplete) {
      m_onComplete();
    }
  }
}

void FadeThroughTransition::render(renderer::IRenderer& renderer) {
  if (!m_running && !m_complete) {
    return;
  }

  f32 progress = getProgress();

  // Alpha goes 0 -> 1 -> 0 (fade out then fade in)
  f32 alpha;
  if (progress < 0.5f) {
    // First half: fade out (0 -> 1)
    f32 t = progress * 2.0f;
    t = t * t * (3.0f - 2.0f * t); // Smooth-step
    alpha = t;
  } else {
    // Second half: fade in (1 -> 0)
    f32 t = (progress - 0.5f) * 2.0f;
    t = t * t * (3.0f - 2.0f * t); // Smooth-step
    alpha = 1.0f - t;
  }

  renderer.setFade(alpha, m_fadeColor);
}

bool FadeThroughTransition::isComplete() const {
  return m_complete;
}

f32 FadeThroughTransition::getProgress() const {
  if (m_duration <= 0.0f) {
    return 1.0f;
  }
  return std::clamp(m_elapsed / m_duration, 0.0f, 1.0f);
}

void FadeThroughTransition::setOnComplete(CompletionCallback callback) {
  m_onComplete = std::move(callback);
}

TransitionType FadeThroughTransition::getType() const {
  return TransitionType::FadeThrough;
}

void FadeThroughTransition::setOnMidpoint(CompletionCallback callback) {
  m_onMidpoint = std::move(callback);
}

bool FadeThroughTransition::isPastMidpoint() const {
  return m_pastMidpoint;
}

// ============================================================================
// SlideTransition
// ============================================================================

SlideTransition::SlideTransition(Direction direction)
    : m_direction(direction), m_duration(1.0f), m_elapsed(0.0f), m_running(false),
      m_complete(false), m_offset(0.0f),
      m_screenSize(1920.0f) // Default, should be set based on actual screen
      ,
      m_onComplete(nullptr) {}

SlideTransition::~SlideTransition() = default;

void SlideTransition::start(f32 duration) {
  m_duration = duration;
  m_elapsed = 0.0f;
  m_running = true;
  m_complete = false;

  // Set initial offset based on direction
  switch (m_direction) {
  case Direction::Left:
    m_offset = m_screenSize;
    break;
  case Direction::Right:
    m_offset = -m_screenSize;
    break;
  case Direction::Up:
    m_offset = m_screenSize;
    break;
  case Direction::Down:
    m_offset = -m_screenSize;
    break;
  }
}

void SlideTransition::update(f64 deltaTime) {
  if (!m_running || m_complete) {
    return;
  }

  m_elapsed += static_cast<f32>(deltaTime);

  if (m_elapsed >= m_duration) {
    m_elapsed = m_duration;
    m_offset = 0.0f;
    m_running = false;
    m_complete = true;

    if (m_onComplete) {
      m_onComplete();
    }
  } else {
    f32 progress = m_elapsed / m_duration;

    // Ease-out cubic
    f32 t = 1.0f - progress;
    t = 1.0f - t * t * t;

    f32 startOffset = 0.0f;
    switch (m_direction) {
    case Direction::Left:
    case Direction::Up:
      startOffset = m_screenSize;
      break;
    case Direction::Right:
    case Direction::Down:
      startOffset = -m_screenSize;
      break;
    }

    m_offset = startOffset * (1.0f - t);
  }
}

void SlideTransition::render(renderer::IRenderer& renderer) {
  // Slide transitions don't directly render anything
  // They provide offset values for the scene to use
  (void)renderer;
}

bool SlideTransition::isComplete() const {
  return m_complete;
}

f32 SlideTransition::getProgress() const {
  if (m_duration <= 0.0f) {
    return 1.0f;
  }
  return std::clamp(m_elapsed / m_duration, 0.0f, 1.0f);
}

void SlideTransition::setOnComplete(CompletionCallback callback) {
  m_onComplete = std::move(callback);
}

TransitionType SlideTransition::getType() const {
  switch (m_direction) {
  case Direction::Left:
    return TransitionType::SlideLeft;
  case Direction::Right:
    return TransitionType::SlideRight;
  case Direction::Up:
    return TransitionType::SlideUp;
  case Direction::Down:
    return TransitionType::SlideDown;
  }
  return TransitionType::SlideLeft;
}

void SlideTransition::setDirection(Direction direction) {
  m_direction = direction;
}

f32 SlideTransition::getOffset() const {
  return m_offset;
}

// ============================================================================
// DissolveTransition
// ============================================================================

DissolveTransition::DissolveTransition()
    : m_duration(1.0f), m_elapsed(0.0f), m_running(false), m_complete(false),
      m_onComplete(nullptr) {}

DissolveTransition::~DissolveTransition() = default;

void DissolveTransition::start(f32 duration) {
  m_duration = duration;
  m_elapsed = 0.0f;
  m_running = true;
  m_complete = false;
}

void DissolveTransition::update(f64 deltaTime) {
  if (!m_running || m_complete) {
    return;
  }

  m_elapsed += static_cast<f32>(deltaTime);

  if (m_elapsed >= m_duration) {
    m_elapsed = m_duration;
    m_running = false;
    m_complete = true;

    if (m_onComplete) {
      m_onComplete();
    }
  }
}

void DissolveTransition::render(renderer::IRenderer& renderer) {
  // Dissolve uses alpha blending controlled by getDissolveAlpha()
  // The actual dissolve pattern would need shader support
  // For now, we can approximate with a simple crossfade
  (void)renderer;
}

bool DissolveTransition::isComplete() const {
  return m_complete;
}

f32 DissolveTransition::getProgress() const {
  if (m_duration <= 0.0f) {
    return 1.0f;
  }
  return std::clamp(m_elapsed / m_duration, 0.0f, 1.0f);
}

void DissolveTransition::setOnComplete(CompletionCallback callback) {
  m_onComplete = std::move(callback);
}

TransitionType DissolveTransition::getType() const {
  return TransitionType::Dissolve;
}

f32 DissolveTransition::getDissolveAlpha() const {
  f32 progress = getProgress();
  // Smooth-step easing
  return progress * progress * (3.0f - 2.0f * progress);
}

// ============================================================================
// WipeTransition
// ============================================================================

WipeTransition::WipeTransition(const renderer::Color& maskColor, Direction direction)
    : m_maskColor(maskColor), m_direction(direction), m_duration(1.0f), m_elapsed(0.0f),
      m_running(false), m_complete(false), m_onComplete(nullptr) {}

WipeTransition::~WipeTransition() = default;

void WipeTransition::start(f32 duration) {
  m_duration = duration;
  m_elapsed = 0.0f;
  m_running = true;
  m_complete = false;
}

void WipeTransition::update(f64 deltaTime) {
  if (!m_running || m_complete) {
    return;
  }

  m_elapsed += static_cast<f32>(deltaTime);
  if (m_elapsed >= m_duration) {
    m_elapsed = m_duration;
    m_running = false;
    m_complete = true;
    if (m_onComplete) {
      m_onComplete();
    }
  }
}

void WipeTransition::render(renderer::IRenderer& renderer) {
  if (!m_running && !m_complete) {
    return;
  }

  const f32 progress = getProgress();
  const f32 width = static_cast<f32>(renderer.getWidth());
  const f32 height = static_cast<f32>(renderer.getHeight());

  renderer::Color color = m_maskColor;
  color.a = static_cast<u8>(color.a * 1.0f);

  renderer::Rect rect{0.0f, 0.0f, width, height};
  switch (m_direction) {
  case Direction::LeftToRight:
    rect.width = width * (1.0f - progress);
    rect.x = width * progress;
    break;
  case Direction::RightToLeft:
    rect.width = width * (1.0f - progress);
    rect.x = 0.0f;
    break;
  case Direction::TopToBottom:
    rect.height = height * (1.0f - progress);
    rect.y = height * progress;
    break;
  case Direction::BottomToTop:
    rect.height = height * (1.0f - progress);
    rect.y = 0.0f;
    break;
  }

  renderer.fillRect(rect, color);
}

bool WipeTransition::isComplete() const {
  return m_complete;
}

f32 WipeTransition::getProgress() const {
  if (m_duration <= 0.0f) {
    return 1.0f;
  }
  return std::clamp(m_elapsed / m_duration, 0.0f, 1.0f);
}

void WipeTransition::setOnComplete(CompletionCallback callback) {
  m_onComplete = std::move(callback);
}

TransitionType WipeTransition::getType() const {
  return TransitionType::Wipe;
}

void WipeTransition::setDirection(Direction direction) {
  m_direction = direction;
}

// ============================================================================
// ZoomTransition
// ============================================================================

ZoomTransition::ZoomTransition(const renderer::Color& maskColor, bool zoomIn)
    : m_maskColor(maskColor), m_zoomIn(zoomIn), m_duration(1.0f), m_elapsed(0.0f), m_running(false),
      m_complete(false), m_onComplete(nullptr) {}

ZoomTransition::~ZoomTransition() = default;

void ZoomTransition::start(f32 duration) {
  m_duration = duration;
  m_elapsed = 0.0f;
  m_running = true;
  m_complete = false;
}

void ZoomTransition::update(f64 deltaTime) {
  if (!m_running || m_complete) {
    return;
  }

  m_elapsed += static_cast<f32>(deltaTime);
  if (m_elapsed >= m_duration) {
    m_elapsed = m_duration;
    m_running = false;
    m_complete = true;
    if (m_onComplete) {
      m_onComplete();
    }
  }
}

void ZoomTransition::render(renderer::IRenderer& renderer) {
  if (!m_running && !m_complete) {
    return;
  }

  const f32 progress = getProgress();
  const f32 width = static_cast<f32>(renderer.getWidth());
  const f32 height = static_cast<f32>(renderer.getHeight());
  const f32 t = m_zoomIn ? (1.0f - progress) : progress;

  const f32 insetX = width * (0.45f * t);
  const f32 insetY = height * (0.45f * t);

  renderer::Color color = m_maskColor;
  renderer.fillRect(renderer::Rect{0.0f, 0.0f, width, insetY}, color);
  renderer.fillRect(renderer::Rect{0.0f, height - insetY, width, insetY}, color);
  renderer.fillRect(renderer::Rect{0.0f, insetY, insetX, height - insetY * 2.0f}, color);
  renderer.fillRect(renderer::Rect{width - insetX, insetY, insetX, height - insetY * 2.0f}, color);
}

bool ZoomTransition::isComplete() const {
  return m_complete;
}

f32 ZoomTransition::getProgress() const {
  if (m_duration <= 0.0f) {
    return 1.0f;
  }
  return std::clamp(m_elapsed / m_duration, 0.0f, 1.0f);
}

void ZoomTransition::setOnComplete(CompletionCallback callback) {
  m_onComplete = std::move(callback);
}

TransitionType ZoomTransition::getType() const {
  return TransitionType::Zoom;
}

void ZoomTransition::setZoomIn(bool zoomIn) {
  m_zoomIn = zoomIn;
}

// ============================================================================
// Factory and utility functions
// ============================================================================

std::unique_ptr<ITransition> createTransition(TransitionType type, const renderer::Color& color) {
  switch (type) {
  case TransitionType::None:
    return nullptr;

  case TransitionType::Fade:
    return std::make_unique<FadeTransition>(color, true);

  case TransitionType::FadeThrough:
    return std::make_unique<FadeThroughTransition>(color);

  case TransitionType::SlideLeft:
    return std::make_unique<SlideTransition>(SlideTransition::Direction::Left);

  case TransitionType::SlideRight:
    return std::make_unique<SlideTransition>(SlideTransition::Direction::Right);

  case TransitionType::SlideUp:
    return std::make_unique<SlideTransition>(SlideTransition::Direction::Up);

  case TransitionType::SlideDown:
    return std::make_unique<SlideTransition>(SlideTransition::Direction::Down);

  case TransitionType::Dissolve:
    return std::make_unique<DissolveTransition>();

  case TransitionType::Wipe:
    return std::make_unique<WipeTransition>(color);

  case TransitionType::Zoom:
    return std::make_unique<ZoomTransition>(color, true);
  }

  return nullptr;
}

TransitionType parseTransitionType(const std::string& name) {
  if (name == "none")
    return TransitionType::None;
  if (name == "fade")
    return TransitionType::Fade;
  if (name == "fadethrough" || name == "fade_through")
    return TransitionType::FadeThrough;
  if (name == "slideleft" || name == "slide_left")
    return TransitionType::SlideLeft;
  if (name == "slideright" || name == "slide_right")
    return TransitionType::SlideRight;
  if (name == "slideup" || name == "slide_up")
    return TransitionType::SlideUp;
  if (name == "slidedown" || name == "slide_down")
    return TransitionType::SlideDown;
  if (name == "dissolve")
    return TransitionType::Dissolve;
  if (name == "wipe")
    return TransitionType::Wipe;
  if (name == "zoom")
    return TransitionType::Zoom;

  return TransitionType::Fade; // Default
}

const char* transitionTypeName(TransitionType type) {
  switch (type) {
  case TransitionType::None:
    return "none";
  case TransitionType::Fade:
    return "fade";
  case TransitionType::FadeThrough:
    return "fadethrough";
  case TransitionType::SlideLeft:
    return "slideleft";
  case TransitionType::SlideRight:
    return "slideright";
  case TransitionType::SlideUp:
    return "slideup";
  case TransitionType::SlideDown:
    return "slidedown";
  case TransitionType::Dissolve:
    return "dissolve";
  case TransitionType::Wipe:
    return "wipe";
  case TransitionType::Zoom:
    return "zoom";
  }
  return "unknown";
}

} // namespace NovelMind::Scene
