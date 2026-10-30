#pragma once

#include "Control.h"
#include "modules/text/TextServer.h"

class Label : public Control {
public:
    using Control::Control;
    void setSize(const glm::vec2& size);
    void setText(std::string text);
    const std::string& getText() const;
    void setFont(Ref<Font> font);
    void setFontSize(float size);
    void setColor(const glm::vec4& color);
    void setWrap(bool wrap);
    void setLineHeight(float lineHeight);
    void setAlignment(TextAlignment alignment);
    void setVerticalAlignment(VerticalTextAlignment alignment);

protected:
    glm::vec2 measure(UIServer& uiServer, const LayoutConstraints& constraints) override;
    void onLayout(UIServer& uiServer, const glm::vec2& availableSize) override;
    void onDraw(UIServer& uiServer, const glm::vec2& globalPosition) override;

private:
    std::string text_;
    Ref<Font> font_;
    glm::vec4 color_{1.0f};
    TextLayout layout_;
    float font_size_ = 18.0f;
    float line_height_ = 0.0f;
    TextAlignment alignment_ = TextAlignment::Left;
    VerticalTextAlignment vertical_alignment_ = VerticalTextAlignment::Top;
    bool wrap_ = true;
};
