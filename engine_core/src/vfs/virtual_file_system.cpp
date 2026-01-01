#include "NovelMind/vfs/virtual_file_system.hpp"
#include <algorithm>

namespace NovelMind::VFS {

namespace {
std::unique_ptr<VirtualFileSystem> g_globalVFS;
std::mutex g_globalVFSMutex;
} // anonymous namespace

VirtualFileSystem::VirtualFileSystem()
    : m_config(),
      m_cache(std::make_unique<ResourceCache>(m_config.cacheMaxSize)) {}

VirtualFileSystem::VirtualFileSystem(const VFSConfig &config)
    : m_config(config),
      m_cache(config.enableCaching
                  ? std::make_unique<ResourceCache>(config.cacheMaxSize)
                  : nullptr) {}

VirtualFileSystem::~VirtualFileSystem() { shutdown(); }

Result<void> VirtualFileSystem::initialize() {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_initialized) {
    return Result<void>::ok();
  }

  for (auto &backend : m_backends) {
    auto result = backend->initialize();
    if (!result.isOk()) {
      return Result<void>::error("Failed to initialize backend '" +
                                 backend->name() + "': " + result.error());
    }
  }

  m_initialized = true;
  return Result<void>::ok();
}

void VirtualFileSystem::shutdown() {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!m_initialized) {
    return;
  }

  if (m_cache) {
    m_cache->clear();
  }

  for (auto &backend : m_backends) {
    backend->shutdown();
  }

  m_initialized = false;
}

void VirtualFileSystem::registerBackend(
    std::unique_ptr<IFileSystemBackend> backend) {
  std::lock_guard<std::mutex> lock(m_mutex);

  if (!backend) {
    return;
  }

  m_backends.push_back(std::move(backend));
  sortBackendsByPriority();
}

void VirtualFileSystem::unregisterBackend(const std::string &name) {
  std::lock_guard<std::mutex> lock(m_mutex);

  m_backends.erase(std::remove_if(m_backends.begin(), m_backends.end(),
                                  [&name](const auto &backend) {
                                    return backend->name() == name;
                                  }),
                   m_backends.end());
}

std::unique_ptr<IFileHandle>
VirtualFileSystem::openStream(const ResourceId &id) {
  // Copy callback and open file while holding lock
  ResourceLoadCallback callback;
  std::unique_ptr<IFileHandle> handle;
  {
    std::lock_guard<std::mutex> lock(m_mutex);

    IFileSystemBackend *backend = findBackend(id);
    if (!backend) {
      return nullptr;
    }

    handle = backend->open(id);
    callback = m_loadCallback;
  }

  // Invoke callback outside lock to avoid deadlock if callback calls VFS methods
  if (callback) {
    callback(id, handle != nullptr);
  }

  return handle;
}

Result<std::vector<u8>> VirtualFileSystem::readAll(const ResourceId &id) {
  if (m_config.enableCaching && m_cache) {
    auto cached = m_cache->get(id);
    if (cached.has_value()) {
      return Result<std::vector<u8>>::ok(std::move(*cached));
    }
  }

  auto handle = openStream(id);
  if (!handle || !handle->isValid()) {
    return Result<std::vector<u8>>::error("Resource not found: " + id.id());
  }

  auto result = handle->readAll();
  if (!result.isOk()) {
    return result;
  }

  if (m_config.enableCaching && m_cache) {
    m_cache->put(id, result.value());
  }

  return result;
}

Result<std::vector<u8>> VirtualFileSystem::readAll(const std::string &id) {
  return readAll(ResourceId(id));
}

bool VirtualFileSystem::exists(const ResourceId &id) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (const auto &backend : m_backends) {
    if (backend->exists(id)) {
      return true;
    }
  }

  return false;
}

bool VirtualFileSystem::exists(const std::string &id) const {
  return exists(ResourceId(id));
}

std::optional<ResourceInfo>
VirtualFileSystem::getInfo(const ResourceId &id) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  for (const auto &backend : m_backends) {
    auto info = backend->getInfo(id);
    if (info.has_value()) {
      return info;
    }
  }

  return std::nullopt;
}

std::vector<ResourceId>
VirtualFileSystem::listResources(ResourceType type) const {
  std::lock_guard<std::mutex> lock(m_mutex);

  std::vector<ResourceId> result;
  for (const auto &backend : m_backends) {
    auto resources = backend->list(type);
    result.insert(result.end(), resources.begin(), resources.end());
  }

  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());

  return result;
}

void VirtualFileSystem::clearCache() {
  if (m_cache) {
    m_cache->clear();
  }
}

void VirtualFileSystem::setCacheMaxSize(usize maxSize) {
  if (m_cache) {
    m_cache->setMaxSize(maxSize);
  }
}

VFSStats VirtualFileSystem::stats() const {
  std::lock_guard<std::mutex> lock(m_mutex);

  VFSStats result;
  result.backendsCount = m_backends.size();

  for (const auto &backend : m_backends) {
    result.totalResources += backend->list().size();
  }

  if (m_cache) {
    result.cacheStats = m_cache->stats();
    result.loadedResources = result.cacheStats.entryCount;
  }

  return result;
}

IFileSystemBackend *VirtualFileSystem::findBackend(const ResourceId &id) const {
  for (const auto &backend : m_backends) {
    if (backend->exists(id)) {
      return backend.get();
    }
  }

  return nullptr;
}

void VirtualFileSystem::sortBackendsByPriority() {
  std::sort(m_backends.begin(), m_backends.end(),
            [](const auto &a, const auto &b) {
              return a->priority() > b->priority();
            });
}

VirtualFileSystem &getGlobalVFS() {
  std::lock_guard<std::mutex> lock(g_globalVFSMutex);

  if (!g_globalVFS) {
    g_globalVFS = std::make_unique<VirtualFileSystem>();
  }

  return *g_globalVFS;
}

void setGlobalVFS(std::unique_ptr<VirtualFileSystem> vfs) {
  std::lock_guard<std::mutex> lock(g_globalVFSMutex);
  g_globalVFS = std::move(vfs);
}

} // namespace NovelMind::VFS
