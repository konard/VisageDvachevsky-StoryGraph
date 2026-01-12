#pragma once

/**
 * @file scene_graph.hpp
 * @brief SceneGraph 2.0 - Full hierarchical scene management
 *
 * Provides a structured scene hierarchy for visual novels:
 * - SceneObjectBase: Common base for all scene objects
 * - Layer hierarchy: Background -> Characters -> UI -> Effects
 * - Full serialization support for Save/Load and Editor
 * - Inspector API for Editor integration
 */

#include "NovelMind/core/result.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/renderer/color.hpp"
#include "NovelMind/renderer/renderer.hpp"
#include "NovelMind/renderer/transform.hpp"
#include "NovelMind/resource/resource_manager.hpp"
#include "NovelMind/scene/animation.hpp"
#include "NovelMind/scene/scene_manager.hpp" // For LayerType enum
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace NovelMind::scene {

// Forward declarations
class SceneGraph;
class Layer;
} // namespace NovelMind::scene

namespace NovelMind::localization {
class LocalizationManager;
}

namespace NovelMind::scene {

/**
 * @brief Type identifiers for scene objects
 */
enum class SceneObjectType : u8 {
  Base,
  Background,
  Character,
  DialogueUI,
  ChoiceUI,
  EffectOverlay,
  Sprite,
  TextLabel,
  Panel,
  Custom
};

/**
 * @brief Serializable state for a scene object
 */
struct SceneObjectState {
  std::string id;
  SceneObjectType type;
  f32 x = 0.0f;
  f32 y = 0.0f;
  f32 width = 100.0f;
  f32 height = 100.0f;
  f32 scaleX = 1.0f;
  f32 scaleY = 1.0f;
  f32 rotation = 0.0f;
  f32 alpha = 1.0f;
  bool visible = true;
  i32 zOrder = 0;
  std::unordered_map<std::string, std::string> properties;
};

/**
 * @brief Property change notification
 */
struct PropertyChange {
  std::string objectId;
  std::string propertyName;
  std::string oldValue;
  std::string newValue;
};

/**
 * @brief Observer interface for scene changes
 */
class ISceneObserver {
public:
  virtual ~ISceneObserver() = default;
  virtual void onObjectAdded(const std::string &objectId,
                             SceneObjectType type) = 0;
  virtual void onObjectRemoved(const std::string &objectId) = 0;
  virtual void onPropertyChanged(const PropertyChange &change) = 0;
  virtual void onLayerChanged(const std::string &objectId,
                              const std::string &newLayer) = 0;
};

/**
 * @brief Base class for all scene objects
 *
 * Provides common functionality:
 * - Transform (position, scale, rotation)
 * - Visibility and alpha
 * - Z-ordering within layer
 * - Property system for serialization
 * - Animation support
 * - Generation counter for thread-safe handle validation
 */
class SceneObjectBase {
public:
  explicit SceneObjectBase(const std::string &id,
                           SceneObjectType type = SceneObjectType::Base);
  virtual ~SceneObjectBase() = default;

  // Non-copyable, movable
  SceneObjectBase(const SceneObjectBase &) = delete;
  SceneObjectBase &operator=(const SceneObjectBase &) = delete;
  SceneObjectBase(SceneObjectBase &&) = default;
  SceneObjectBase &operator=(SceneObjectBase &&) = default;

  // Identity
  [[nodiscard]] const std::string &getId() const { return m_id; }
  [[nodiscard]] SceneObjectType getType() const { return m_type; }
  [[nodiscard]] const char *getTypeName() const;
  [[nodiscard]] u64 getGeneration() const { return m_generation; }

  // Transform
  void setPosition(f32 x, f32 y);
  void setScale(f32 scaleX, f32 scaleY);
  void setUniformScale(f32 scale);
  void setRotation(f32 angle);
  void setAnchor(f32 anchorX, f32 anchorY);

  [[nodiscard]] f32 getX() const { return m_transform.x; }
  [[nodiscard]] f32 getY() const { return m_transform.y; }
  [[nodiscard]] f32 getScaleX() const { return m_transform.scaleX; }
  [[nodiscard]] f32 getScaleY() const { return m_transform.scaleY; }
  [[nodiscard]] f32 getRotation() const { return m_transform.rotation; }
  [[nodiscard]] f32 getAnchorX() const { return m_anchorX; }
  [[nodiscard]] f32 getAnchorY() const { return m_anchorY; }
  [[nodiscard]] const renderer::Transform2D &getTransform() const {
    return m_transform;
  }

  // Visibility
  void setVisible(bool visible);
  void setAlpha(f32 alpha);
  [[nodiscard]] bool isVisible() const { return m_visible; }
  [[nodiscard]] f32 getAlpha() const { return m_alpha; }

  // Z-ordering
  void setZOrder(i32 zOrder);
  [[nodiscard]] i32 getZOrder() const { return m_zOrder; }

  // Parent/child relationship
  void setParent(SceneObjectBase *parent);
  [[nodiscard]] SceneObjectBase *getParent() const { return m_parent; }
  void addChild(std::unique_ptr<SceneObjectBase> child);
  std::unique_ptr<SceneObjectBase> removeChild(const std::string &id);
  [[nodiscard]] SceneObjectBase *findChild(const std::string &id);
  [[nodiscard]] const std::vector<std::unique_ptr<SceneObjectBase>> &
  getChildren() const {
    return m_children;
  }

  /// Maximum depth for recursive scene graph traversal to prevent stack
  /// overflow
  static constexpr int MAX_SCENE_DEPTH = 100;

  // Tags for filtering
  void addTag(const std::string &tag);
  void removeTag(const std::string &tag);
  [[nodiscard]] bool hasTag(const std::string &tag) const;
  [[nodiscard]] const std::vector<std::string> &getTags() const {
    return m_tags;
  }

  // Property system (for serialization and editor)
  void setProperty(const std::string &name, const std::string &value);
  [[nodiscard]] std::optional<std::string>
  getProperty(const std::string &name) const;
  [[nodiscard]] const std::unordered_map<std::string, std::string> &
  getProperties() const {
    return m_properties;
  }

  // Lifecycle
  virtual void update(f64 deltaTime);
  virtual void render(renderer::IRenderer &renderer) = 0;

  // Serialization
  [[nodiscard]] virtual SceneObjectState saveState() const;
  virtual void loadState(const SceneObjectState &state);

  // Animation support
  void animatePosition(f32 toX, f32 toY, f32 duration,
                       EaseType easing = EaseType::Linear);
  void animateAlpha(f32 toAlpha, f32 duration,
                    EaseType easing = EaseType::Linear);
  void animateScale(f32 toScaleX, f32 toScaleY, f32 duration,
                    EaseType easing = EaseType::Linear);

protected:
  // Notify observers of property changes
  void notifyPropertyChanged(const std::string &property,
                             const std::string &oldValue,
                             const std::string &newValue);

  // Protected access for derived classes
  renderer::Transform2D m_transform;
  f32 m_anchorX = 0.5f;
  f32 m_anchorY = 0.5f;
  f32 m_alpha = 1.0f;
  bool m_visible = true;
  resource::ResourceManager *m_resources = nullptr;
  localization::LocalizationManager *m_localization = nullptr;

private:
  // Recursive helper with depth limit to prevent stack overflow
  SceneObjectBase *findChildRecursive(const std::string &id, int depth);

  std::string m_id;
  SceneObjectType m_type;
  i32 m_zOrder = 0;
  u64 m_generation = 0; // Generation counter for thread-safe handle validation

  SceneObjectBase *m_parent = nullptr;
  std::vector<std::unique_ptr<SceneObjectBase>> m_children;
  std::vector<std::string> m_tags;
  std::unordered_map<std::string, std::string> m_properties;

  // Active animations
  std::vector<std::unique_ptr<Tween>> m_animations;

  // Observer for change notifications (set by SceneGraph)
  ISceneObserver *m_observer = nullptr;
  friend class SceneGraph;
};

/**
 * @brief Background object - full screen image
 */
class BackgroundObject : public SceneObjectBase {
public:
  explicit BackgroundObject(const std::string &id);

  void setTextureId(const std::string &textureId);
  [[nodiscard]] const std::string &getTextureId() const { return m_textureId; }

  void setTint(const renderer::Color &color);
  [[nodiscard]] const renderer::Color &getTint() const { return m_tint; }

  void render(renderer::IRenderer &renderer) override;
  [[nodiscard]] SceneObjectState saveState() const override;
  void loadState(const SceneObjectState &state) override;

private:
  std::string m_textureId;
  renderer::Color m_tint{255, 255, 255, 255};
};

/**
 * @brief Character object - character sprite with expressions
 */
class CharacterObject : public SceneObjectBase {
public:
  /**
   * @brief Character slot positions
   */
  enum class Position : u8 { Left, Center, Right, Custom };

  explicit CharacterObject(const std::string &id,
                           const std::string &characterId);

  void setCharacterId(const std::string &characterId);
  [[nodiscard]] const std::string &getCharacterId() const {
    return m_characterId;
  }

  void setDisplayName(const std::string &name);
  [[nodiscard]] const std::string &getDisplayName() const {
    return m_displayName;
  }

  void setExpression(const std::string &expression);
  [[nodiscard]] const std::string &getExpression() const {
    return m_expression;
  }

  void setPose(const std::string &pose);
  [[nodiscard]] const std::string &getPose() const { return m_pose; }

  void setSlotPosition(Position pos);
  [[nodiscard]] Position getSlotPosition() const { return m_slotPosition; }

  void setNameColor(const renderer::Color &color);
  [[nodiscard]] const renderer::Color &getNameColor() const {
    return m_nameColor;
  }

  void setHighlighted(bool highlighted);
  [[nodiscard]] bool isHighlighted() const { return m_highlighted; }

  void render(renderer::IRenderer &renderer) override;
  [[nodiscard]] SceneObjectState saveState() const override;
  void loadState(const SceneObjectState &state) override;

  // Animation
  void animateToSlot(Position slot, f32 duration,
                     EaseType easing = EaseType::EaseOutQuad);

private:
  std::string m_characterId;
  std::string m_displayName;
  std::string m_expression = "default";
  std::string m_pose = "default";
  Position m_slotPosition = Position::Center;
  renderer::Color m_nameColor{255, 255, 255, 255};
  bool m_highlighted = false;
};

/**
 * @brief Dialogue UI object - text box with speaker name
 */
class DialogueUIObject : public SceneObjectBase {
public:
  explicit DialogueUIObject(const std::string &id);

  void setSpeaker(const std::string &speaker);
  [[nodiscard]] const std::string &getSpeaker() const { return m_speaker; }

  void setText(const std::string &text);
  [[nodiscard]] const std::string &getText() const { return m_text; }

  void setSpeakerColor(const renderer::Color &color);
  [[nodiscard]] const renderer::Color &getSpeakerColor() const {
    return m_speakerColor;
  }

  void setBackgroundTextureId(const std::string &textureId);
  [[nodiscard]] const std::string &getBackgroundTextureId() const {
    return m_backgroundTextureId;
  }

  // Typewriter effect
  void setTypewriterEnabled(bool enabled);
  [[nodiscard]] bool isTypewriterEnabled() const { return m_typewriterEnabled; }
  void setTypewriterSpeed(f32 charsPerSecond);
  [[nodiscard]] f32 getTypewriterSpeed() const { return m_typewriterSpeed; }

  void startTypewriter();
  void skipTypewriter();
  [[nodiscard]] bool isTypewriterComplete() const {
    return m_typewriterComplete;
  }

  void update(f64 deltaTime) override;
  void render(renderer::IRenderer &renderer) override;
  [[nodiscard]] SceneObjectState saveState() const override;
  void loadState(const SceneObjectState &state) override;

private:
  std::string m_speaker;
  std::string m_text;
  renderer::Color m_speakerColor{255, 255, 255, 255};
  std::string m_backgroundTextureId;

  // Typewriter state
  bool m_typewriterEnabled = true;
  f32 m_typewriterSpeed = 30.0f;
  f32 m_typewriterProgress = 0.0f;
  bool m_typewriterComplete = true;
};

/**
 * @brief Choice UI object - choice menu
 */
class ChoiceUIObject : public SceneObjectBase {
public:
  struct ChoiceOption {
    std::string id;
    std::string text;
    bool enabled = true;
    bool visible = true;
    std::string condition;
  };

  explicit ChoiceUIObject(const std::string &id);

  void setChoices(const std::vector<ChoiceOption> &choices);
  [[nodiscard]] const std::vector<ChoiceOption> &getChoices() const {
    return m_choices;
  }
  void clearChoices();

  void setSelectedIndex(i32 index);
  [[nodiscard]] i32 getSelectedIndex() const { return m_selectedIndex; }

  void selectNext();
  void selectPrevious();
  [[nodiscard]] bool confirm();

  void setOnSelect(std::function<void(i32, const std::string &)> callback);

  void render(renderer::IRenderer &renderer) override;
  [[nodiscard]] SceneObjectState saveState() const override;
  void loadState(const SceneObjectState &state) override;

private:
  std::vector<ChoiceOption> m_choices;
  i32 m_selectedIndex = 0;
  std::function<void(i32, const std::string &)> m_onSelect;
};

/**
 * @brief Effect overlay object - visual effects layer
 */
class EffectOverlayObject : public SceneObjectBase {
public:
  enum class EffectType : u8 { None, Fade, Flash, Shake, Rain, Snow, Custom };

  explicit EffectOverlayObject(const std::string &id);

  void setEffectType(EffectType type);
  [[nodiscard]] EffectType getEffectType() const { return m_effectType; }

  void setColor(const renderer::Color &color);
  [[nodiscard]] const renderer::Color &getColor() const { return m_color; }

  void setIntensity(f32 intensity);
  [[nodiscard]] f32 getIntensity() const { return m_intensity; }

  void startEffect(f32 duration);
  void stopEffect();
  [[nodiscard]] bool isEffectActive() const { return m_effectActive; }

  void update(f64 deltaTime) override;
  void render(renderer::IRenderer &renderer) override;
  [[nodiscard]] SceneObjectState saveState() const override;
  void loadState(const SceneObjectState &state) override;

private:
  EffectType m_effectType = EffectType::None;
  renderer::Color m_color{0, 0, 0, 255};
  f32 m_intensity = 1.0f;
  bool m_effectActive = false;
  f32 m_effectTimer = 0.0f;
  f32 m_effectDuration = 0.0f;
};

// LayerType is defined in scene_manager.hpp
// Using forward declaration to avoid duplication
// Note: This file assumes LayerType is already defined when included

/**
 * @brief Layer - container for scene objects of the same category
 */
class Layer {
public:
  explicit Layer(const std::string &name, LayerType type);

  [[nodiscard]] const std::string &getName() const { return m_name; }
  [[nodiscard]] LayerType getType() const { return m_type; }

  void addObject(std::unique_ptr<SceneObjectBase> object);
  std::unique_ptr<SceneObjectBase> removeObject(const std::string &id);
  void clear();

  // Non-owning pointer; valid only until the next scene mutation.
  [[nodiscard]] SceneObjectBase *findObject(const std::string &id);
  [[nodiscard]] const SceneObjectBase *findObject(const std::string &id) const;
  [[nodiscard]] const std::vector<std::unique_ptr<SceneObjectBase>> &
  getObjects() const {
    return m_objects;
  }

  void setVisible(bool visible);
  [[nodiscard]] bool isVisible() const { return m_visible; }

  void setAlpha(f32 alpha);
  [[nodiscard]] f32 getAlpha() const { return m_alpha; }

  void sortByZOrder();

  void update(f64 deltaTime);
  void render(renderer::IRenderer &renderer);

private:
  std::string m_name;
  LayerType m_type;
  std::vector<std::unique_ptr<SceneObjectBase>> m_objects;
  bool m_visible = true;
  f32 m_alpha = 1.0f;
};

/**
 * @brief Serializable scene state
 */
struct SceneState {
  std::string sceneId;
  std::vector<SceneObjectState> objects;
  std::string activeBackground;
  std::vector<std::string> visibleCharacters;
};

/**
 * @brief SceneGraph - main scene management class
 *
 * Manages the complete scene hierarchy:
 * - Four layers: Background, Characters, UI, Effects
 * - Object lifecycle (add, remove, find)
 * - Full serialization for Save/Load
 * - Observer pattern for Editor integration
 */
class SceneGraph : public ISceneObserver {
public:
  SceneGraph();
  ~SceneGraph() override;

  // Scene management
  void setSceneId(const std::string &id);
  [[nodiscard]] const std::string &getSceneId() const { return m_sceneId; }
  void clear();

  // Layer access
  [[nodiscard]] Layer &getBackgroundLayer() { return m_backgroundLayer; }
  [[nodiscard]] Layer &getCharacterLayer() { return m_characterLayer; }
  [[nodiscard]] Layer &getUILayer() { return m_uiLayer; }
  [[nodiscard]] Layer &getEffectLayer() { return m_effectLayer; }
  [[nodiscard]] Layer &getLayer(LayerType type);

  // Object management
  void addToLayer(LayerType layer, std::unique_ptr<SceneObjectBase> object);
  std::unique_ptr<SceneObjectBase> removeFromLayer(LayerType layer,
                                                   const std::string &id);
  [[nodiscard]] SceneObjectBase *findObject(const std::string &id);
  [[nodiscard]] std::vector<SceneObjectBase *>
  findObjectsByTag(const std::string &tag);
  [[nodiscard]] std::vector<SceneObjectBase *>
  findObjectsByType(SceneObjectType type);

  // Thread-safe object access for handles
  // Returns nullptr if object not found or generation mismatch
  [[nodiscard]] SceneObjectBase *findObjectWithGeneration(const std::string &id,
                                                          u64 generation);

  // Get mutex for external synchronization (for handle withObject methods)
  std::mutex &getObjectMutex() { return m_objectMutex; }

  // Convenience methods for common operations
  void showBackground(const std::string &textureId);
  CharacterObject *showCharacter(const std::string &id,
                                 const std::string &characterId,
                                 CharacterObject::Position position);
  void hideCharacter(const std::string &id);
  DialogueUIObject *showDialogue(const std::string &speaker,
                                 const std::string &text);
  void hideDialogue();
  ChoiceUIObject *
  showChoices(const std::vector<ChoiceUIObject::ChoiceOption> &choices);
  void hideChoices();

  // Update and render
  void update(f64 deltaTime);
  void render(renderer::IRenderer &renderer);

  void setResourceManager(resource::ResourceManager *resources);
  [[nodiscard]] resource::ResourceManager *getResourceManager() const {
    return m_resources;
  }

  void setLocalizationManager(localization::LocalizationManager *localization);
  [[nodiscard]] localization::LocalizationManager *
  getLocalizationManager() const {
    return m_localization;
  }

  // Serialization
  [[nodiscard]] SceneState saveState() const;
  void loadState(const SceneState &state);

  // Observer management
  void addObserver(ISceneObserver *observer);
  void removeObserver(ISceneObserver *observer);

  // ISceneObserver implementation (for internal use)
  void onObjectAdded(const std::string &objectId,
                     SceneObjectType type) override;
  void onObjectRemoved(const std::string &objectId) override;
  void onPropertyChanged(const PropertyChange &change) override;
  void onLayerChanged(const std::string &objectId,
                      const std::string &newLayer) override;

private:
  void notifyObservers(const std::function<void(ISceneObserver *)> &notify);
  void registerObject(SceneObjectBase *obj);

  std::string m_sceneId;
  Layer m_backgroundLayer;
  Layer m_characterLayer;
  Layer m_uiLayer;
  Layer m_effectLayer;

  std::vector<ISceneObserver *> m_observers;
  resource::ResourceManager *m_resources = nullptr;
  localization::LocalizationManager *m_localization = nullptr;

  // Thread-safety: Protects access to scene objects via handles
  // Used for TOCTOU-safe findObject operations and withObject callbacks
  mutable std::mutex m_objectMutex;
};

} // namespace NovelMind::scene
