#pragma once

#include "Control.h"

class Expanded : public Control {
public:
    using Control::Control;
    void setFlex(float flex);
    float getFlex() const;

protected:
    void layoutChildren(UIServer& uiServer, const glm::vec2& availableSize) override;

private:
    float flex_ = 1.0f;
};
