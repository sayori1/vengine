#pragma once

#include <cstdint>
#include <memory>
#include <string>

template<typename T>
using Ref = std::shared_ptr<T>;

class Resource {
public:
    using UID = uint64_t;

    Resource() = default;
    virtual ~Resource() = default;

    Resource(const Resource&) = delete;
    Resource& operator=(const Resource&) = delete;

    UID getUID() const;
    const std::string& getPath() const;

    bool isExternal() const;

private:
    friend class ResourceManager;

    void setUID(UID uid);
    void setPath(std::string path);

    UID uid_ = 0;
    std::string path_;
};
