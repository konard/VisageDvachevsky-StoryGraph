/**
 * @file scene_object_properties.cpp
 * @brief Property registration implementation for scene objects
 *
 * Implements type-safe property binding for all scene object types,
 * enabling the Inspector to edit real object properties without
 * raw pointer hazards.
 */

#include "NovelMind/scene/scene_object_properties.hpp"
#include "NovelMind/renderer/color.hpp"

namespace NovelMind::scene {

// ============================================================================
// Helper: Convert renderer::Color to/from property system Color
// ============================================================================

static Color toPropertyColor(const renderer::Color& rc) {
  return Color(static_cast<f32>(rc.r) / 255.0f, static_cast<f32>(rc.g) / 255.0f,
               static_cast<f32>(rc.b) / 255.0f, static_cast<f32>(rc.a) / 255.0f);
}

static renderer::Color fromPropertyColor(const Color& c) {
  return renderer::Color(static_cast<u8>(c.r * 255.0f), static_cast<u8>(c.g * 255.0f),
                         static_cast<u8>(c.b * 255.0f), static_cast<u8>(c.a * 255.0f));
}

// ============================================================================
// SceneObjectBase Properties
// ============================================================================

void registerSceneObjectBaseProperties() {
  PropertyMeta xMeta{"x", "X Position", PropertyType::Float};
  xMeta.category = "Transform";
  xMeta.tooltip = "Horizontal position in pixels";
  xMeta.order = 1;

  PropertyMeta yMeta{"y", "Y Position", PropertyType::Float};
  yMeta.category = "Transform";
  yMeta.tooltip = "Vertical position in pixels";
  yMeta.order = 2;

  PropertyMeta scaleXMeta{"scaleX", "Scale X", PropertyType::Float};
  scaleXMeta.category = "Transform";
  scaleXMeta.tooltip = "Horizontal scale factor";
  scaleXMeta.range = RangeConstraint(0.0, 10.0);
  scaleXMeta.defaultValue = 1.0f;
  scaleXMeta.order = 3;

  PropertyMeta scaleYMeta{"scaleY", "Scale Y", PropertyType::Float};
  scaleYMeta.category = "Transform";
  scaleYMeta.tooltip = "Vertical scale factor";
  scaleYMeta.range = RangeConstraint(0.0, 10.0);
  scaleYMeta.defaultValue = 1.0f;
  scaleYMeta.order = 4;

  PropertyMeta rotationMeta{"rotation", "Rotation", PropertyType::Float};
  rotationMeta.category = "Transform";
  rotationMeta.tooltip = "Rotation angle in degrees";
  rotationMeta.flags = PropertyFlags::Angle;
  rotationMeta.order = 5;

  PropertyMeta alphaMeta{"alpha", "Alpha", PropertyType::Float};
  alphaMeta.category = "Appearance";
  alphaMeta.tooltip = "Opacity (0 = transparent, 1 = opaque)";
  alphaMeta.range = RangeConstraint(0.0, 1.0);
  alphaMeta.flags = PropertyFlags::Slider | PropertyFlags::Normalized;
  alphaMeta.defaultValue = 1.0f;
  alphaMeta.order = 1;

  PropertyMeta visibleMeta{"visible", "Visible", PropertyType::Bool};
  visibleMeta.category = "Appearance";
  visibleMeta.tooltip = "Whether the object is visible";
  visibleMeta.defaultValue = true;
  visibleMeta.order = 2;

  PropertyMeta zOrderMeta{"zOrder", "Z-Order", PropertyType::Int};
  zOrderMeta.category = "Rendering";
  zOrderMeta.tooltip = "Draw order within layer (higher = on top)";
  zOrderMeta.order = 1;

  TypeInfoBuilder<SceneObjectBase>("SceneObjectBase")
      .property<f32>(
          xMeta, [](const SceneObjectBase& obj) { return obj.getX(); },
          [](SceneObjectBase& obj, const f32& val) { obj.setPosition(val, obj.getY()); })
      .property<f32>(
          yMeta, [](const SceneObjectBase& obj) { return obj.getY(); },
          [](SceneObjectBase& obj, const f32& val) { obj.setPosition(obj.getX(), val); })
      .property<f32>(
          scaleXMeta, [](const SceneObjectBase& obj) { return obj.getScaleX(); },
          [](SceneObjectBase& obj, const f32& val) { obj.setScale(val, obj.getScaleY()); })
      .property<f32>(
          scaleYMeta, [](const SceneObjectBase& obj) { return obj.getScaleY(); },
          [](SceneObjectBase& obj, const f32& val) { obj.setScale(obj.getScaleX(), val); })
      .property<f32>(
          rotationMeta, [](const SceneObjectBase& obj) { return obj.getRotation(); },
          [](SceneObjectBase& obj, const f32& val) { obj.setRotation(val); })
      .property<f32>(
          alphaMeta, [](const SceneObjectBase& obj) { return obj.getAlpha(); },
          [](SceneObjectBase& obj, const f32& val) { obj.setAlpha(val); })
      .property<bool>(
          visibleMeta, [](const SceneObjectBase& obj) { return obj.isVisible(); },
          [](SceneObjectBase& obj, const bool& val) { obj.setVisible(val); })
      .property<i32>(
          zOrderMeta, [](const SceneObjectBase& obj) { return obj.getZOrder(); },
          [](SceneObjectBase& obj, const i32& val) { obj.setZOrder(val); })
      .build();
}

// ============================================================================
// BackgroundObject Properties
// ============================================================================

void registerBackgroundObjectProperties() {
  PropertyMeta textureIdMeta{"textureId", "Texture", PropertyType::AssetRef};
  textureIdMeta.category = "Background";
  textureIdMeta.tooltip = "Background texture asset";
  textureIdMeta.assetFilter = "*.png;*.jpg;*.jpeg;*.bmp";
  textureIdMeta.flags = PropertyFlags::FilePicker;
  textureIdMeta.order = 1;

  PropertyMeta tintMeta{"tint", "Tint Color", PropertyType::Color};
  tintMeta.category = "Background";
  tintMeta.tooltip = "Color tint applied to the background";
  tintMeta.flags = PropertyFlags::ColorPicker;
  tintMeta.defaultValue = Color(1.0f, 1.0f, 1.0f, 1.0f);
  tintMeta.order = 2;

  TypeInfoBuilder<BackgroundObject>("BackgroundObject")
      .property<AssetRef>(
          textureIdMeta,
          [](const BackgroundObject& obj) {
            return AssetRef{"texture", obj.getTextureId()};
          },
          [](BackgroundObject& obj, const AssetRef& val) { obj.setTextureId(val.path); })
      .property<Color>(
          tintMeta, [](const BackgroundObject& obj) { return toPropertyColor(obj.getTint()); },
          [](BackgroundObject& obj, const Color& val) { obj.setTint(fromPropertyColor(val)); })
      .build();
}

// ============================================================================
// CharacterObject Properties
// ============================================================================

void registerCharacterObjectProperties() {
  PropertyMeta characterIdMeta{"characterId", "Character ID", PropertyType::String};
  characterIdMeta.category = "Character";
  characterIdMeta.tooltip = "Unique character identifier";
  characterIdMeta.flags = PropertyFlags::ReadOnly;
  characterIdMeta.order = 1;

  PropertyMeta displayNameMeta{"displayName", "Display Name", PropertyType::String};
  displayNameMeta.category = "Character";
  displayNameMeta.tooltip = "Character name shown in dialogue";
  displayNameMeta.order = 2;

  PropertyMeta expressionMeta{"expression", "Expression", PropertyType::String};
  expressionMeta.category = "Character";
  expressionMeta.tooltip = "Current facial expression";
  expressionMeta.order = 3;

  PropertyMeta poseMeta{"pose", "Pose", PropertyType::String};
  poseMeta.category = "Character";
  poseMeta.tooltip = "Current character pose/outfit";
  poseMeta.order = 4;

  PropertyMeta slotPositionMeta{"slotPosition", "Slot Position", PropertyType::Enum};
  slotPositionMeta.category = "Position";
  slotPositionMeta.tooltip = "Pre-defined screen position slot";
  slotPositionMeta.enumOptions = {{0, "Left"}, {1, "Center"}, {2, "Right"}, {3, "Custom"}};
  slotPositionMeta.order = 1;

  PropertyMeta highlightedMeta{"highlighted", "Highlighted", PropertyType::Bool};
  highlightedMeta.category = "Appearance";
  highlightedMeta.tooltip = "Whether the character is highlighted (speaking)";
  highlightedMeta.defaultValue = false;
  highlightedMeta.order = 3;

  PropertyMeta nameColorMeta{"nameColor", "Name Color", PropertyType::Color};
  nameColorMeta.category = "Appearance";
  nameColorMeta.tooltip = "Color of the character's name in dialogue";
  nameColorMeta.flags = PropertyFlags::ColorPicker;
  nameColorMeta.defaultValue = Color(1.0f, 1.0f, 1.0f, 1.0f);
  nameColorMeta.order = 4;

  TypeInfoBuilder<CharacterObject>("CharacterObject")
      .property<std::string>(
          characterIdMeta, [](const CharacterObject& obj) { return obj.getCharacterId(); },
          [](CharacterObject& obj, const std::string& val) { obj.setCharacterId(val); })
      .property<std::string>(
          displayNameMeta, [](const CharacterObject& obj) { return obj.getDisplayName(); },
          [](CharacterObject& obj, const std::string& val) { obj.setDisplayName(val); })
      .property<std::string>(
          expressionMeta, [](const CharacterObject& obj) { return obj.getExpression(); },
          [](CharacterObject& obj, const std::string& val) { obj.setExpression(val); })
      .property<std::string>(
          poseMeta, [](const CharacterObject& obj) { return obj.getPose(); },
          [](CharacterObject& obj, const std::string& val) { obj.setPose(val); })
      .property<EnumValue>(
          slotPositionMeta,
          [](const CharacterObject& obj) {
            return EnumValue(static_cast<i32>(obj.getSlotPosition()), "");
          },
          [](CharacterObject& obj, const EnumValue& val) {
            obj.setSlotPosition(static_cast<CharacterObject::Position>(val.value));
          })
      .property<bool>(
          highlightedMeta, [](const CharacterObject& obj) { return obj.isHighlighted(); },
          [](CharacterObject& obj, const bool& val) { obj.setHighlighted(val); })
      .property<Color>(
          nameColorMeta,
          [](const CharacterObject& obj) { return toPropertyColor(obj.getNameColor()); },
          [](CharacterObject& obj, const Color& val) { obj.setNameColor(fromPropertyColor(val)); })
      .build();
}

// ============================================================================
// DialogueUIObject Properties
// ============================================================================

void registerDialogueUIObjectProperties() {
  PropertyMeta speakerMeta{"speaker", "Speaker", PropertyType::String};
  speakerMeta.category = "Dialogue";
  speakerMeta.tooltip = "Name of the speaking character";
  speakerMeta.order = 1;

  PropertyMeta textMeta{"text", "Text", PropertyType::String};
  textMeta.category = "Dialogue";
  textMeta.tooltip = "Dialogue text content";
  textMeta.flags = PropertyFlags::MultiLine;
  textMeta.order = 2;

  PropertyMeta speakerColorMeta{"speakerColor", "Speaker Color", PropertyType::Color};
  speakerColorMeta.category = "Appearance";
  speakerColorMeta.tooltip = "Color of the speaker name";
  speakerColorMeta.flags = PropertyFlags::ColorPicker;
  speakerColorMeta.defaultValue = Color(1.0f, 1.0f, 1.0f, 1.0f);
  speakerColorMeta.order = 1;

  PropertyMeta bgTextureMeta{"backgroundTextureId", "Background Texture", PropertyType::AssetRef};
  bgTextureMeta.category = "Appearance";
  bgTextureMeta.tooltip = "Dialogue box background texture";
  bgTextureMeta.assetFilter = "*.png;*.jpg;*.jpeg;*.bmp";
  bgTextureMeta.flags = PropertyFlags::FilePicker;
  bgTextureMeta.order = 2;

  PropertyMeta typewriterEnabledMeta{"typewriterEnabled", "Typewriter Effect", PropertyType::Bool};
  typewriterEnabledMeta.category = "Typewriter";
  typewriterEnabledMeta.tooltip = "Enable character-by-character text reveal";
  typewriterEnabledMeta.defaultValue = true;
  typewriterEnabledMeta.order = 1;

  PropertyMeta typewriterSpeedMeta{"typewriterSpeed", "Typewriter Speed", PropertyType::Float};
  typewriterSpeedMeta.category = "Typewriter";
  typewriterSpeedMeta.tooltip = "Characters per second for typewriter effect";
  typewriterSpeedMeta.range = RangeConstraint(1.0, 200.0);
  typewriterSpeedMeta.flags = PropertyFlags::Slider;
  typewriterSpeedMeta.defaultValue = 30.0f;
  typewriterSpeedMeta.order = 2;

  TypeInfoBuilder<DialogueUIObject>("DialogueUIObject")
      .property<std::string>(
          speakerMeta, [](const DialogueUIObject& obj) { return obj.getSpeaker(); },
          [](DialogueUIObject& obj, const std::string& val) { obj.setSpeaker(val); })
      .property<std::string>(
          textMeta, [](const DialogueUIObject& obj) { return obj.getText(); },
          [](DialogueUIObject& obj, const std::string& val) { obj.setText(val); })
      .property<Color>(
          speakerColorMeta,
          [](const DialogueUIObject& obj) { return toPropertyColor(obj.getSpeakerColor()); },
          [](DialogueUIObject& obj, const Color& val) {
            obj.setSpeakerColor(fromPropertyColor(val));
          })
      .property<AssetRef>(
          bgTextureMeta,
          [](const DialogueUIObject& obj) {
            return AssetRef{"texture", obj.getBackgroundTextureId()};
          },
          [](DialogueUIObject& obj, const AssetRef& val) { obj.setBackgroundTextureId(val.path); })
      .property<bool>(
          typewriterEnabledMeta,
          [](const DialogueUIObject& obj) { return obj.isTypewriterEnabled(); },
          [](DialogueUIObject& obj, const bool& val) { obj.setTypewriterEnabled(val); })
      .property<f32>(
          typewriterSpeedMeta, [](const DialogueUIObject& obj) { return obj.getTypewriterSpeed(); },
          [](DialogueUIObject& obj, const f32& val) { obj.setTypewriterSpeed(val); })
      .build();
}

// ============================================================================
// EffectOverlayObject Properties
// ============================================================================

void registerEffectOverlayObjectProperties() {
  PropertyMeta effectTypeMeta{"effectType", "Effect Type", PropertyType::Enum};
  effectTypeMeta.category = "Effect";
  effectTypeMeta.tooltip = "Type of visual effect";
  effectTypeMeta.enumOptions = {{0, "None"}, {1, "Fade"}, {2, "Flash"}, {3, "Shake"},
                                {4, "Rain"}, {5, "Snow"}, {6, "Custom"}};
  effectTypeMeta.order = 1;

  PropertyMeta colorMeta{"color", "Effect Color", PropertyType::Color};
  colorMeta.category = "Effect";
  colorMeta.tooltip = "Color of the effect";
  colorMeta.flags = PropertyFlags::ColorPicker;
  colorMeta.defaultValue = Color(0.0f, 0.0f, 0.0f, 1.0f);
  colorMeta.order = 2;

  PropertyMeta intensityMeta{"intensity", "Intensity", PropertyType::Float};
  intensityMeta.category = "Effect";
  intensityMeta.tooltip = "Effect intensity/strength";
  intensityMeta.range = RangeConstraint(0.0, 1.0);
  intensityMeta.flags = PropertyFlags::Slider | PropertyFlags::Normalized;
  intensityMeta.defaultValue = 1.0f;
  intensityMeta.order = 3;

  TypeInfoBuilder<EffectOverlayObject>("EffectOverlayObject")
      .property<EnumValue>(
          effectTypeMeta,
          [](const EffectOverlayObject& obj) {
            return EnumValue(static_cast<i32>(obj.getEffectType()), "");
          },
          [](EffectOverlayObject& obj, const EnumValue& val) {
            obj.setEffectType(static_cast<EffectOverlayObject::EffectType>(val.value));
          })
      .property<Color>(
          colorMeta, [](const EffectOverlayObject& obj) { return toPropertyColor(obj.getColor()); },
          [](EffectOverlayObject& obj, const Color& val) { obj.setColor(fromPropertyColor(val)); })
      .property<f32>(
          intensityMeta, [](const EffectOverlayObject& obj) { return obj.getIntensity(); },
          [](EffectOverlayObject& obj, const f32& val) { obj.setIntensity(val); })
      .build();
}

// ============================================================================
// Master Registration Function
// ============================================================================

void registerSceneObjectProperties() {
  registerSceneObjectBaseProperties();
  registerBackgroundObjectProperties();
  registerCharacterObjectProperties();
  registerDialogueUIObjectProperties();
  registerEffectOverlayObjectProperties();
}

} // namespace NovelMind::scene
