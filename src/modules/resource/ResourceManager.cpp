#include "ResourceManager.h"

std::mutex ResourceManager::mutex_;

std::unordered_map<
    ResourceManager::Key,
    std::weak_ptr<Resource>,
    ResourceManager::KeyHash
> ResourceManager::cache_;

std::unordered_map<
    std::type_index,
    ResourceManager::GenericLoader
> ResourceManager::loaders_;

void ResourceManager::clearUnused() {
    std::lock_guard lock(mutex_);

    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->second.expired()) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void ResourceManager::clearCache() {
    std::lock_guard lock(mutex_);
    cache_.clear();
}
