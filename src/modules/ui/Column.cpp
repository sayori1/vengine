#include "Column.h"

#include "Expanded.h"
#include "UIServer.h"

#include <algorithm>
#include <vector>

void Column::setSpacing(float spacing) {
    spacing_ = std::max(0.0f, spacing);
    markLayoutDirty();
}

void Column::setAlignment(MainAxisAlignment alignment) {
    alignment_ = alignment;
    markLayoutDirty();
}

void Column::layoutChildren(UIServer& uiServer, const glm::vec2& availableSize) {
    std::vector<Control*> children;
    float fixedHeight = 0.0f;
    float totalFlex = 0.0f;
    for (const auto& child : getChildren()) {
        auto* control = dynamic_cast<Control*>(child.get());
        if (!control) continue;
        children.push_back(control);
        if (auto* expanded = dynamic_cast<Expanded*>(control)) totalFlex += expanded->getFlex();
        else fixedHeight += control->measure(uiServer, {glm::vec2(0.0f), availableSize}).y;
    }
    if (children.empty()) return;
    fixedHeight += spacing_ * static_cast<float>(children.size() - 1);
    const float freeHeight = std::max(0.0f, availableSize.y - fixedHeight);
    float gap = spacing_;
    float cursor = alignment_ == MainAxisAlignment::Center ? freeHeight * 0.5f :
                   alignment_ == MainAxisAlignment::End ? freeHeight : 0.0f;
    if (alignment_ == MainAxisAlignment::SpaceBetween && children.size() > 1) {
        gap += freeHeight / static_cast<float>(children.size() - 1);
    }
    for (Control* child : children) {
        float height = child->measure(uiServer, {glm::vec2(0.0f), availableSize}).y;
        if (auto* expanded = dynamic_cast<Expanded*>(child)) {
            height = totalFlex > 0.0f ? freeHeight * expanded->getFlex() / totalFlex : 0.0f;
        }
        child->setPosition({0.0f, cursor});
        child->setLayoutSize({availableSize.x, height});
        cursor += height + gap;
    }
}
