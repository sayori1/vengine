#include "Expanded.h"
#include "UIServer.h"

#include <algorithm>

void Expanded::setFlex(float flex) {
    flex_ = std::max(0.0f, flex);
    markLayoutDirty();
}

float Expanded::getFlex() const { return flex_; }

void Expanded::layoutChildren(UIServer&, const glm::vec2& availableSize) {
    for (const auto& child : getChildren()) {
        if (auto* control = dynamic_cast<Control*>(child.get())) {
            control->setPosition({0.0f, 0.0f});
            control->setLayoutSize(availableSize);
        }
    }
}
