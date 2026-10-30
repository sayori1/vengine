#include "Row.h"

#include "Expanded.h"
#include "UIServer.h"

#include <algorithm>
#include <vector>

void Row::setSpacing(float spacing) {
    spacing_ = std::max(0.0f, spacing);
    markLayoutDirty();
}

void Row::setAlignment(MainAxisAlignment alignment) {
    alignment_ = alignment;
    markLayoutDirty();
}

void Row::layoutChildren(UIServer& uiServer, const glm::vec2& availableSize) {
    std::vector<Control*> children;
    float fixedWidth = 0.0f;
    float totalFlex = 0.0f;
    for (const auto& child : getChildren()) {
        auto* control = dynamic_cast<Control*>(child.get());
        if (!control) continue;
        children.push_back(control);
        if (auto* expanded = dynamic_cast<Expanded*>(control)) totalFlex += expanded->getFlex();
        else fixedWidth += control->measure(uiServer, {glm::vec2(0.0f), availableSize}).x;
    }
    if (children.empty()) return;
    fixedWidth += spacing_ * static_cast<float>(children.size() - 1);
    const float freeWidth = std::max(0.0f, availableSize.x - fixedWidth);
    float gap = spacing_;
    float cursor = alignment_ == MainAxisAlignment::Center ? freeWidth * 0.5f :
                   alignment_ == MainAxisAlignment::End ? freeWidth : 0.0f;
    if (alignment_ == MainAxisAlignment::SpaceBetween && children.size() > 1) {
        gap += freeWidth / static_cast<float>(children.size() - 1);
    }
    for (Control* child : children) {
        float width = child->measure(uiServer, {glm::vec2(0.0f), availableSize}).x;
        if (auto* expanded = dynamic_cast<Expanded*>(child)) {
            width = totalFlex > 0.0f ? freeWidth * expanded->getFlex() / totalFlex : 0.0f;
        }
        child->setPosition({cursor, 0.0f});
        child->setLayoutSize({width, availableSize.y});
        cursor += width + gap;
    }
}
