#include "UIServer.h"

#include "Control.h"
#include "modules/renderer/RenderingServer.h"
#include "modules/text/TextServer.h"

#include <algorithm>

void UIServer::registerRoot(Control& control) {
    if (std::find(roots_.begin(), roots_.end(), &control) == roots_.end()) {
        roots_.push_back(&control);
    }
}

void UIServer::unregisterRoot(Control& control) {
    std::erase(roots_, &control);
}

void UIServer::update(const glm::vec2& viewportSize) {
    for (Control* root : roots_) {
        if (root) {
            layout(*root, glm::vec2(0.0f), viewportSize);
            draw(*root, 0);
        }
    }
    if (text_server_) {
        RenderingServer::get().setGlyphAtlas(text_server_->getSharedAtlasPixels(),
                                              text_server_->getAtlasVersion());
    }
}

void UIServer::layout(Control& control, const glm::vec2& parentPosition,
                      const glm::vec2& availableSize) {
    if (!control.visible_) {
        return;
    }
    control.size_ = control.measure(*this, {glm::vec2(0.0f), availableSize});
    glm::vec2 contentSize(
        std::max(0.0f, control.size_.x - control.padding_.left - control.padding_.right),
        std::max(0.0f, control.size_.y - control.padding_.top - control.padding_.bottom));
    if (control.layout_dirty_ || control.last_available_size_ != availableSize) {
        control.onLayout(*this, contentSize);
        control.size_ = control.measure(*this, {glm::vec2(0.0f), availableSize});
        contentSize = {
            std::max(0.0f, control.size_.x - control.padding_.left - control.padding_.right),
            std::max(0.0f, control.size_.y - control.padding_.top - control.padding_.bottom)
        };
        control.layoutChildren(*this, contentSize);
        control.layout_dirty_ = false;
        control.last_available_size_ = availableSize;
    }
    control.global_position_ = parentPosition + control.position_;
    layoutDescendants(control,
                      control.global_position_ + glm::vec2(control.padding_.left, control.padding_.top),
                      contentSize);
}

void UIServer::layoutDescendants(Node& node, const glm::vec2& parentPosition,
                                 const glm::vec2& availableSize) {
    for (const auto& child : node.getChildren()) {
        if (auto* nested = dynamic_cast<Control*>(child.get())) {
            layout(*nested, parentPosition, availableSize);
        } else {
            layoutDescendants(*child, parentPosition, availableSize);
        }
    }
}

void UIServer::draw(Control& control, int parentZ) {
    if (!control.visible_) {
        return;
    }
    const int z = parentZ + control.z_index_;
    control.draw_z_index_ = z;
    control.onDraw(*this, control.global_position_);
    drawDescendants(control, z);
}

void UIServer::drawDescendants(Node& node, int parentZ) {
    for (const auto& child : node.getChildren()) {
        if (auto* nested = dynamic_cast<Control*>(child.get())) {
            draw(*nested, parentZ);
        } else {
            drawDescendants(*child, parentZ);
        }
    }
}

void UIServer::drawRect(const glm::vec2& position, const glm::vec2& size,
                        const glm::vec4& color, int zIndex) {
    RenderingServer::get().drawRect(position, size, color, zIndex);
}

void UIServer::setTextServer(TextServer& textServer, Ref<Font> defaultFont) {
    text_server_ = &textServer;
    default_font_ = std::move(defaultFont);
}

TextServer& UIServer::getTextServer() { return *text_server_; }
const Ref<Font>& UIServer::getDefaultFont() const { return default_font_; }

void UIServer::drawGlyph(const glm::vec2& position, const glm::vec2& size,
                         const glm::vec4& color, const glm::vec4& uv, int zIndex) {
    RenderingServer::get().drawGlyph(position, size, color, uv, zIndex);
}
