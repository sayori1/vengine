#pragma once

#include "Control.h"

#include <glm/vec4.hpp>

class Panel : public Control {
public:
    using Control::Control;
    void setBackgroundColor(const glm::vec4& color);
    const glm::vec4& getBackgroundColor() const;

protected:
    void onDraw(UIServer& uiServer, const glm::vec2& globalPosition) override;

private:
    glm::vec4 background_color_{0.2f, 0.2f, 0.2f, 1.0f};
};
