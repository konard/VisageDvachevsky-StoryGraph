#pragma once

#include "NovelMind/audio/audio_manager.hpp"
#include "NovelMind/core/result.hpp"
#include "NovelMind/core/timer.hpp"
#include "NovelMind/core/types.hpp"
#include "NovelMind/input/input_manager.hpp"
#include "NovelMind/localization/localization_manager.hpp"
#include "NovelMind/platform/file_system.hpp"
#include "NovelMind/platform/window.hpp"
#include "NovelMind/renderer/renderer.hpp"
#include "NovelMind/resource/resource_manager.hpp"
#include "NovelMind/save/save_manager.hpp"
#include "NovelMind/scene/scene_graph.hpp"
#include "NovelMind/vfs/virtual_fs.hpp"
#include <atomic>
#include <memory>
#include <string>

namespace NovelMind::core {

struct EngineConfig {
  platform::WindowConfig window;
  std::string packFile;
  std::string startScene;
  bool debug = false;
};

class Application {
public:
  Application();
  ~Application();

  Application(const Application &) = delete;
  Application &operator=(const Application &) = delete;

  Result<void> initialize(const EngineConfig &config);
  void shutdown();

  void run();
  void quit();

  [[nodiscard]] bool isRunning() const;
  [[nodiscard]] f64 getDeltaTime() const;
  [[nodiscard]] f64 getElapsedTime() const;

  [[nodiscard]] platform::IWindow *getWindow();
  [[nodiscard]] const platform::IWindow *getWindow() const;
  [[nodiscard]] platform::IFileSystem *getFileSystem();
  [[nodiscard]] renderer::IRenderer *getRenderer();
  [[nodiscard]] resource::ResourceManager *getResources();
  [[nodiscard]] scene::SceneGraph *getSceneGraph();
  [[nodiscard]] input::InputManager *getInput();
  [[nodiscard]] audio::AudioManager *getAudio();
  [[nodiscard]] save::SaveManager *getSaveManager();
  [[nodiscard]] localization::LocalizationManager *getLocalization();

protected:
  virtual void onInitialize();
  virtual void onShutdown();
  virtual void onUpdate(f64 deltaTime);
  virtual void onRender();

private:
  void mainLoop();

  std::atomic<bool> m_running{false};
  EngineConfig m_config;

  std::unique_ptr<platform::IWindow> m_window;
  std::unique_ptr<platform::IFileSystem> m_fileSystem;
  std::unique_ptr<vfs::IVirtualFileSystem> m_vfs;
  std::unique_ptr<renderer::IRenderer> m_renderer;
  std::unique_ptr<resource::ResourceManager> m_resources;
  std::unique_ptr<scene::SceneGraph> m_sceneGraph;
  std::unique_ptr<input::InputManager> m_input;
  std::unique_ptr<audio::AudioManager> m_audio;
  std::unique_ptr<save::SaveManager> m_saveManager;
  std::unique_ptr<localization::LocalizationManager> m_localization;
  Timer m_timer;
};

} // namespace NovelMind::core
