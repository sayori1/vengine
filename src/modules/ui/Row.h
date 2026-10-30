#pragma once

#include "Control.h"

enum class MainAxisAlignment {
    Start,
    Center,
    End,
    SpaceBetween
};

class Row : public Control {
public:
    using Control::Control;
    void setSpacing(float spacing);
    void setAlignment(MainAxisAlignment alignment);

protected:
    void layoutChildren(UIServer& uiServer, const glm::vec2& availableSize) override;

private:
    float spacing_ = 0.0f;
    MainAxisAlignment alignment_ = MainAxisAlignment::Start;
};
