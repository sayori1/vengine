#include "Padding.h"
#include "UIServer.h"

#include <algorithm>

void Padding::setPadding(const glm::vec4& padding) {
    padding_ = glm::max(padding, glm::vec4(0.0f));
    markLayoutDirty();
}

void Padding::layoutChildren(UIServer&, const glm::vec2& availableSize) {
    const glm::vec2 childSize(
        std::max(0.0f, availableSize.x - padding_.x - padding_.z),
        std::max(0.0f, availableSize.y - padding_.y - padding_.w));
    for (const auto& child : getChildren()) {
        if (auto* control = dynamic_cast<Control*>(child.get())) {
            control->setPosition({padding_.x, padding_.y});
            control->setLayoutSize(childSize);
        }
    }
}
