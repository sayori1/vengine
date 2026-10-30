#include "Stack.h"
#include "UIServer.h"

void Stack::layoutChildren(UIServer& uiServer, const glm::vec2& availableSize) {
    for (const auto& child : getChildren()) {
        if (auto* control = dynamic_cast<Control*>(child.get())) {
            control->setLayoutSize(control->measure(uiServer, {glm::vec2(0.0f), availableSize}));
        }
    }
}
