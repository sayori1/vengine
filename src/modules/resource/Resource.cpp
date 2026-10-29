#include "Resource.h"

#include <utility>

Resource::UID Resource::getUID() const {
    return uid_;
}

const std::string& Resource::getPath() const {
    return path_;
}

bool Resource::isExternal() const {
    return !path_.empty();
}

void Resource::setUID(UID uid) {
    uid_ = uid;
}

void Resource::setPath(std::string path) {
    path_ = std::move(path);
}
