/**
 * @file scene_object_dialogue.cpp
 * @brief Dialogue UI scene object implementation
 */

#include "NovelMind/scene/scene_graph.hpp"

#include <algorithm>

#include "NovelMind/localization/localization_manager.hpp"
#include "NovelMind/renderer/text_layout.hpp"

#include "scene_graph_detail.hpp"

namespace NovelMind::scene {

// ============================================================================
// DialogueUIObject Implementation
// ============================================================================

DialogueUIObject::DialogueUIObject(const std::string& id)
    : SceneObjectBase(id, SceneObjectType::DialogueUI) {}

void DialogueUIObject::setSpeaker(const std::string& speaker) {
  m_speaker = speaker;
}

void DialogueUIObject::setText(const std::string& text) {
  m_text = text;
  m_typewriterProgress = 0.0f;
  m_typewriterComplete = !m_typewriterEnabled;
}

void DialogueUIObject::setSpeakerColor(const renderer::Color& color) {
  m_speakerColor = color;
}

void DialogueUIObject::setBackgroundTextureId(const std::string& textureId) {
  m_backgroundTextureId = textureId;
}

void DialogueUIObject::setTypewriterEnabled(bool enabled) {
  m_typewriterEnabled = enabled;
}

void DialogueUIObject::setTypewriterSpeed(f32 charsPerSecond) {
  m_typewriterSpeed = charsPerSecond;
}

void DialogueUIObject::startTypewriter() {
  m_typewriterProgress = 0.0f;
  m_typewriterComplete = false;
}

void DialogueUIObject::skipTypewriter() {
  m_typewriterProgress = static_cast<f32>(m_text.length());
  m_typewriterComplete = true;
}

void DialogueUIObject::update(f64 deltaTime) {
  SceneObjectBase::update(deltaTime);

  if (m_typewriterEnabled && !m_typewriterComplete) {
    m_typewriterProgress += static_cast<f32>(deltaTime) * m_typewriterSpeed;
    if (m_typewriterProgress >= static_cast<f32>(m_text.length())) {
      m_typewriterProgress = static_cast<f32>(m_text.length());
      m_typewriterComplete = true;
    }
  }
}

void DialogueUIObject::render(renderer::IRenderer& renderer) {
  if (!m_visible || m_alpha <= 0.0f) {
    return;
  }

  if (!m_resources) {
    return;
  }

  const bool rtl = detail::parseBool(
      getProperty("rtl"), m_localization ? m_localization->isCurrentLocaleRightToLeft() : false);
  renderer::TextAlign align = rtl ? renderer::TextAlign::Right : renderer::TextAlign::Left;

  const float width = detail::parseFloat(getProperty("width"), detail::kDefaultDialogueWidth);
  const float height = detail::parseFloat(getProperty("height"), detail::kDefaultDialogueHeight);
  const float padding = detail::parseFloat(getProperty("padding"), detail::kDefaultDialoguePadding);

  renderer::Rect rect{m_transform.x - width * m_anchorX, m_transform.y - height * m_anchorY, width,
                      height};

  if (!m_backgroundTextureId.empty()) {
    auto texResult = m_resources->loadTexture(m_backgroundTextureId);
    if (texResult.isOk() && texResult.value()->isValid()) {
      const auto& texture = *texResult.value();
      renderer::Transform2D transform{};
      transform.x = rect.x;
      transform.y = rect.y;
      transform.scaleX = rect.width / static_cast<f32>(texture.getWidth());
      transform.scaleY = rect.height / static_cast<f32>(texture.getHeight());
      transform.anchorX = 0.0f;
      transform.anchorY = 0.0f;
      renderer::Color tint = renderer::Color::White;
      tint.a = static_cast<u8>(tint.a * m_alpha);
      renderer.drawSprite(texture, transform, tint);
    }
  } else {
    renderer::Color bg = renderer::Color(30, 30, 30, 200);
    bg.a = static_cast<u8>(bg.a * m_alpha);
    renderer.fillRect(rect, bg);
  }

  std::string fontId = detail::getTextProperty(*this, "fontId", detail::defaultFontPath());
  i32 fontSize = static_cast<i32>(detail::parseFloat(getProperty("fontSize"), 18.0f));
  if (!fontId.empty()) {
    auto fontResult = m_resources->loadFont(fontId, fontSize);
    if (fontResult.isOk()) {
      auto atlasResult = m_resources->loadFontAtlas(fontId, fontSize,
                                                    " !\"#$%&'()*+,-./0123456789:;<=>?"
                                                    "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
                                                    "`abcdefghijklmnopqrstuvwxyz{|}~");
      if (atlasResult.isOk()) {
        renderer::TextLayoutEngine layout;
        layout.setFont(fontResult.value());
        layout.setFontAtlas(atlasResult.value());
        layout.setMaxWidth(rect.width - padding * 2.0f);
        layout.setAlignment(align);
        layout.setRightToLeft(rtl);
        renderer::TextStyle style;
        style.color = renderer::Color::White;
        style.size = static_cast<f32>(fontSize);
        layout.setDefaultStyle(style);

        std::string visible = m_text;
        if (m_typewriterEnabled) {
          size_t count =
              static_cast<size_t>(std::min(m_typewriterProgress, static_cast<f32>(m_text.size())));
          visible = m_text.substr(0, count);
        }

        renderer::TextLayout textLayout = layout.layout(visible);
        f32 y = rect.y + padding + static_cast<f32>(fontSize);
        for (const auto& line : textLayout.lines) {
          f32 x = rect.x + padding;
          if (align == renderer::TextAlign::Center) {
            x = rect.x + (rect.width - line.width) * 0.5f;
          } else if (align == renderer::TextAlign::Right) {
            x = rect.x + rect.width - padding;
          }

          if (!rtl) {
            for (const auto& segment : line.segments) {
              if (segment.isCommand()) {
                continue;
              }
              renderer.drawText(*fontResult.value(), segment.text, x, y, segment.style.color);
              x += layout.measureText(segment.text).first;
            }
          } else {
            for (auto it = line.segments.rbegin(); it != line.segments.rend(); ++it) {
              if (it->isCommand()) {
                continue;
              }
              const f32 segWidth = layout.measureText(it->text).first;
              x -= segWidth;
              renderer.drawText(*fontResult.value(), it->text, x, y, it->style.color);
            }
          }
          y += line.height;
        }
      }
    }
  }

  if (!m_speaker.empty()) {
    std::string speakerFontId = detail::getTextProperty(*this, "speakerFontId", fontId);
    i32 speakerFontSize = static_cast<i32>(
        detail::parseFloat(getProperty("speakerFontSize"), static_cast<float>(fontSize + 2)));
    if (!speakerFontId.empty()) {
      auto fontResult = m_resources->loadFont(speakerFontId, speakerFontSize);
      if (fontResult.isOk()) {
        f32 speakerX = rect.x + padding;
        if (rtl) {
          renderer::TextLayoutEngine speakerLayout;
          speakerLayout.setFont(fontResult.value());
          renderer::TextStyle speakerStyle;
          speakerStyle.size = static_cast<f32>(speakerFontSize);
          speakerLayout.setDefaultStyle(speakerStyle);
          f32 speakerWidth = speakerLayout.measureText(m_speaker).first;
          speakerX = rect.x + rect.width - padding - speakerWidth;
        }
        renderer.drawText(*fontResult.value(), m_speaker, speakerX, rect.y + padding,
                          m_speakerColor);
      }
    }
  }
}

SceneObjectState DialogueUIObject::saveState() const {
  auto state = SceneObjectBase::saveState();
  state.properties["speaker"] = m_speaker;
  state.properties["text"] = m_text;
  state.properties["backgroundTextureId"] = m_backgroundTextureId;
  state.properties["typewriterEnabled"] = m_typewriterEnabled ? "true" : "false";
  state.properties["typewriterSpeed"] = std::to_string(m_typewriterSpeed);
  return state;
}

void DialogueUIObject::loadState(const SceneObjectState& state) {
  SceneObjectBase::loadState(state);

  auto it = state.properties.find("speaker");
  if (it != state.properties.end())
    m_speaker = it->second;

  it = state.properties.find("text");
  if (it != state.properties.end())
    m_text = it->second;

  it = state.properties.find("backgroundTextureId");
  if (it != state.properties.end())
    m_backgroundTextureId = it->second;

  it = state.properties.find("typewriterEnabled");
  if (it != state.properties.end()) {
    m_typewriterEnabled = (it->second == "true");
  }

  it = state.properties.find("typewriterSpeed");
  if (it != state.properties.end()) {
    m_typewriterSpeed = std::stof(it->second);
  }
}

} // namespace NovelMind::scene
