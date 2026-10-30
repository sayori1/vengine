#pragma once

#include "Control.h"
#include <glm/vec4.hpp>

class Padding : public Control {
public:
    using Control::Control;
    void setPadding(const glm::vec4& padding);

protected:
    void layoutChildren(UIServer& uiServer, const glm::vec2& availableSize) override;

private:
    glm::vec4 padding_{0.0f};
};
