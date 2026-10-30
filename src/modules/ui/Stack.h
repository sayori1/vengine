#pragma once

#include "Control.h"

class Stack : public Control {
public:
    using Control::Control;

protected:
    void layoutChildren(UIServer& uiServer, const glm::vec2& availableSize) override;
};
