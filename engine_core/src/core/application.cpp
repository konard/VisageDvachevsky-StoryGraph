#include "NovelMind/core/application.hpp"
#include "NovelMind/core/logger.hpp"
#include "NovelMind/vfs/cached_file_system.hpp"
#include "NovelMind/vfs/memory_fs.hpp"
#include "NovelMind/vfs/secure_pack_reader.hpp"

namespace NovelMind::core {

Application::Application() = default;

Application::~Application() { shutdown(); }

Result<void> Application::initialize(const EngineConfig &config) {
  m_config = config;

  if (m_config.debug) {
    Logger::instance().setLevel(LogLevel::Debug);
  }

  NOVELMIND_LOG_INFO("Initializing NovelMind engine...");

  m_window = platform::createWindow();
  auto windowResult = m_window->create(m_config.window);
  if (windowResult.isError()) {
    NOVELMIND_LOG_ERROR("Failed to create window");
    return windowResult;
  }

  m_fileSystem = platform::createFileSystem();

  std::unique_ptr<vfs::IVirtualFileSystem> baseFs;
  if (!m_config.packFile.empty()) {
    auto packFs = std::make_unique<vfs::SecurePackFileSystem>();
    auto mountResult = packFs->mount(m_config.packFile);
    if (mountResult.isError()) {
      return Result<void>::error(mountResult.error());
    }
    baseFs = std::move(packFs);
  } else {
    baseFs = std::make_unique<vfs::MemoryFileSystem>();
  }
  m_vfs = std::make_unique<vfs::CachedFileSystem>(std::move(baseFs));

  m_renderer = renderer::createRenderer();
  auto renderResult = m_renderer->initialize(*m_window);
  if (renderResult.isError()) {
    return Result<void>::error(renderResult.error());
  }

  m_resources = std::make_unique<resource::ResourceManager>(m_vfs.get());
  m_sceneGraph = std::make_unique<scene::SceneGraph>();
  m_sceneGraph->setResourceManager(m_resources.get());

  m_input = std::make_unique<input::InputManager>();

  m_audio = std::make_unique<audio::AudioManager>();
  m_audio->setDataProvider([this](const std::string &id) {
    if (!m_resources) {
      return Result<std::vector<u8>>::error("Resource manager unavailable");
    }
    return m_resources->readData(id);
  });
  m_audio->initialize();

  m_saveManager = std::make_unique<save::SaveManager>();
  m_localization = std::make_unique<localization::LocalizationManager>();
  if (m_sceneGraph) {
    m_sceneGraph->setLocalizationManager(m_localization.get());
  }

  m_timer.reset();
  m_running = true;

  onInitialize();

  NOVELMIND_LOG_INFO("Engine initialized successfully");
  return Result<void>::ok();
}

void Application::shutdown() {
  if (!m_running) {
    return;
  }

  NOVELMIND_LOG_INFO("Shutting down engine...");

  onShutdown();

  if (m_audio) {
    m_audio->shutdown();
  }
  m_audio.reset();
  m_sceneGraph.reset();
  m_resources.reset();
  if (m_renderer) {
    m_renderer->shutdown();
  }
  m_renderer.reset();
  m_vfs.reset();
  m_saveManager.reset();
  m_localization.reset();
  m_input.reset();
  m_fileSystem.reset();

  if (m_window) {
    m_window->destroy();
    m_window.reset();
  }

  m_running = false;

  NOVELMIND_LOG_INFO("Engine shutdown complete");
}

void Application::run() {
  if (!m_running) {
    NOVELMIND_LOG_ERROR("Cannot run: engine not initialized");
    return;
  }

  NOVELMIND_LOG_INFO("Starting main loop...");
  mainLoop();
}

void Application::quit() { m_running = false; }

bool Application::isRunning() const { return m_running; }

f64 Application::getDeltaTime() const { return m_timer.getDeltaTime(); }

f64 Application::getElapsedTime() const { return m_timer.getElapsedSeconds(); }

platform::IWindow *Application::getWindow() { return m_window.get(); }

const platform::IWindow *Application::getWindow() const {
  return m_window.get();
}

platform::IFileSystem *Application::getFileSystem() {
  return m_fileSystem.get();
}

renderer::IRenderer *Application::getRenderer() { return m_renderer.get(); }

resource::ResourceManager *Application::getResources() {
  return m_resources.get();
}

scene::SceneGraph *Application::getSceneGraph() { return m_sceneGraph.get(); }

input::InputManager *Application::getInput() { return m_input.get(); }

audio::AudioManager *Application::getAudio() { return m_audio.get(); }

save::SaveManager *Application::getSaveManager() { return m_saveManager.get(); }

localization::LocalizationManager *Application::getLocalization() {
  return m_localization.get();
}

void Application::onInitialize() {
  // Override in derived class
}

void Application::onShutdown() {
  // Override in derived class
}

void Application::onUpdate(f64 /*deltaTime*/) {
  // Override in derived class
}

void Application::onRender() {
  // Override in derived class
}

void Application::mainLoop() {
  while (m_running && !m_window->shouldClose()) {
    m_timer.tick();
    f64 deltaTime = m_timer.getDeltaTime();

    m_window->pollEvents();

    onUpdate(deltaTime);
    if (m_input) {
      m_input->update();
    }

    if (m_sceneGraph) {
      m_sceneGraph->update(deltaTime);
    }
    if (m_audio) {
      m_audio->update(deltaTime);
    }

    if (m_renderer) {
      m_renderer->beginFrame();
    }

    onRender();
    if (m_renderer && m_sceneGraph) {
      m_sceneGraph->render(*m_renderer);
    }

    if (m_renderer) {
      m_renderer->endFrame();
    }

    if (!m_renderer) {
      m_window->swapBuffers();
    }
  }
}

} // namespace NovelMind::core
