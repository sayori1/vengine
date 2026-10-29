#pragma once

#include "Resource.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>

class ResourceManager {
public:
    template<typename T>
    using Loader = std::function<Ref<T>(const std::string&)>;

    template<typename T>
    static void registerLoader(Loader<T> loader) {
        std::lock_guard lock(mutex_);

        loaders_[std::type_index(typeid(T))] =
            [loader = std::move(loader)](const std::string& path) -> Ref<Resource> {
                return loader(path);
            };
    }

    template<typename T>
    static Ref<T> load(const std::string& path) {
        static_assert(std::is_base_of_v<Resource, T>);

        std::lock_guard lock(mutex_);

        const Key key{std::type_index(typeid(T)), path};

        if (auto it = cache_.find(key); it != cache_.end()) {
            if (auto resource = it->second.lock()) {
                return std::static_pointer_cast<T>(resource);
            }

            cache_.erase(it);
        }

        auto loader = loaders_.find(key.type);

        if (loader == loaders_.end()) {
            return nullptr;
        }

        auto resource = loader->second(path);

        if (!resource) {
            return nullptr;
        }

        resource->setPath(path);
        cache_[key] = resource;

        return std::static_pointer_cast<T>(resource);
    }

    static void clearUnused();
    static void clearCache();

private:
    struct Key {
        std::type_index type;
        std::string path;

        bool operator==(const Key& other) const {
            return type == other.type && path == other.path;
        }
    };

    struct KeyHash {
        size_t operator()(const Key& key) const {
            const size_t h1 = std::hash<std::type_index>{}(key.type);
            const size_t h2 = std::hash<std::string>{}(key.path);

            return h1 ^ (h2 << 1);
        }
    };

    using GenericLoader = std::function<Ref<Resource>(const std::string&)>;

    static std::mutex mutex_;
    static std::unordered_map<Key, std::weak_ptr<Resource>, KeyHash> cache_;
    static std::unordered_map<std::type_index, GenericLoader> loaders_;
};
