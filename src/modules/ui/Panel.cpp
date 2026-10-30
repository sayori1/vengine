#include "Panel.h"

#include "UIServer.h"

void Panel::setBackgroundColor(const glm::vec4& color) { background_color_ = color; }
const glm::vec4& Panel::getBackgroundColor() const { return background_color_; }

void Panel::onDraw(UIServer& uiServer, const glm::vec2& globalPosition) {
    uiServer.drawRect(globalPosition, getSize(), background_color_, getDrawZIndex());
}
