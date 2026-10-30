#include "Control.h"

#include "UIServer.h"

void Control::setPosition(const glm::vec2& position) {
    position_ = position;
    markLayoutDirty();
}

void Control::setSize(const glm::vec2& size) {
    size_ = size;
    explicit_size_ = size != glm::vec2(0.0f);
    markLayoutDirty();
}

void Control::setVisible(bool visible) {
    visible_ = visible;
}

void Control::setZIndex(int zIndex) {
    z_index_ = zIndex;
}

void Control::setPadding(const EdgeInsets& padding) {
    padding_ = {std::max(0.0f, padding.left), std::max(0.0f, padding.top),
                std::max(0.0f, padding.right), std::max(0.0f, padding.bottom)};
    markLayoutDirty();
}

const EdgeInsets& Control::getPadding() const { return padding_; }

void Control::setMinimumSize(const glm::vec2& size) {
    minimum_size_ = glm::max(size, glm::vec2(0.0f));
    markLayoutDirty();
}

void Control::setMaximumSize(const glm::vec2& size) {
    maximum_size_ = glm::max(size, minimum_size_);
    markLayoutDirty();
}

glm::vec2 Control::measure(const LayoutConstraints& constraints) const {
    const glm::vec2 minimum = glm::max(constraints.minimum, minimum_size_);
    const glm::vec2 maximum = glm::max(minimum, glm::min(constraints.maximum, maximum_size_));
    const glm::vec2 preferred(
        size_.x == 0.0f ? maximum.x : size_.x,
        size_.y == 0.0f ? maximum.y : size_.y);
    return glm::clamp(preferred, minimum, maximum);
}

glm::vec2 Control::measure(UIServer&, const LayoutConstraints& constraints) {
    return measure(constraints);
}

const glm::vec2& Control::getPosition() const { return position_; }
const glm::vec2& Control::getSize() const { return size_; }
bool Control::isVisible() const { return visible_; }
int Control::getZIndex() const { return z_index_; }
glm::vec2 Control::getGlobalPosition() const { return global_position_; }

void Control::onReady() {
    bool hasControlParent = false;
    for (Node* parent = getParent(); parent; parent = parent->getParent()) {
        if (dynamic_cast<Control*>(parent)) {
            hasControlParent = true;
            break;
        }
    }
    if (!hasControlParent && getUIServer()) {
        getUIServer()->registerRoot(*this);
    }
}

void Control::onDestroy() {
    if (getUIServer()) {
        getUIServer()->unregisterRoot(*this);
    }
}

void Control::onDraw(UIServer&, const glm::vec2&) {}
void Control::onLayout(UIServer&, const glm::vec2&) {}
void Control::layoutChildren(UIServer&, const glm::vec2&) {}
void Control::markLayoutDirty() { layout_dirty_ = true; }
int Control::getDrawZIndex() const { return draw_z_index_; }
bool Control::hasExplicitSize() const { return explicit_size_; }
void Control::setLayoutSize(const glm::vec2& size) {
    if (size_ != size) {
        size_ = size;
        markLayoutDirty();
    }
}

glm::vec2 Control::constrainSize(const glm::vec2& preferred,
                                 const LayoutConstraints& constraints) const {
    const glm::vec2 minimum = glm::max(constraints.minimum, minimum_size_);
    const glm::vec2 maximum = glm::max(minimum, glm::min(constraints.maximum, maximum_size_));
    return glm::clamp(preferred, minimum, maximum);
}
