#include "Label.h"

#include "UIServer.h"

#include <algorithm>
#include <utility>

void Label::setText(std::string text) { text_ = std::move(text); markLayoutDirty(); }
void Label::setSize(const glm::vec2& size) {
    Control::setSize(size);
}
const std::string& Label::getText() const { return text_; }
void Label::setFont(Ref<Font> font) { font_ = std::move(font); markLayoutDirty(); }
void Label::setFontSize(float size) { font_size_ = size; markLayoutDirty(); }
void Label::setColor(const glm::vec4& color) { color_ = color; }
void Label::setWrap(bool wrap) { wrap_ = wrap; markLayoutDirty(); }
void Label::setLineHeight(float lineHeight) { line_height_ = lineHeight; markLayoutDirty(); }
void Label::setAlignment(TextAlignment alignment) { alignment_ = alignment; markLayoutDirty(); }
void Label::setVerticalAlignment(VerticalTextAlignment alignment) {
    vertical_alignment_ = alignment;
}

glm::vec2 Label::measure(UIServer& uiServer, const LayoutConstraints& constraints) {
    if (hasExplicitSize()) return Control::measure(uiServer, constraints);
    const EdgeInsets& padding = getPadding();
    const float contentWidth = std::max(0.0f, constraints.maximum.x - padding.left - padding.right);
    const auto& selectedFont = font_ ? font_ : uiServer.getDefaultFont();
    layout_ = uiServer.getTextServer().layout(selectedFont, font_size_, text_,
                                               contentWidth, wrap_, line_height_, alignment_);
    return constrainSize({constraints.maximum.x, layout_.size.y + padding.top + padding.bottom},
                         constraints);
}

void Label::onLayout(UIServer& uiServer, const glm::vec2& availableSize) {
    const auto& selectedFont = font_ ? font_ : uiServer.getDefaultFont();
    layout_ = uiServer.getTextServer().layout(selectedFont, font_size_, text_,
                                               availableSize.x, wrap_, line_height_, alignment_);
}

void Label::onDraw(UIServer& uiServer, const glm::vec2& globalPosition) {
    const float remainingHeight = std::max(0.0f, getSize().y - layout_.size.y);
    const float verticalOffset = vertical_alignment_ == VerticalTextAlignment::Center
        ? remainingHeight * 0.5f
        : vertical_alignment_ == VerticalTextAlignment::Bottom ? remainingHeight : 0.0f;
    for (const auto& glyph : layout_.glyphs) {
        if (glyph.size.x > 0.0f && glyph.size.y > 0.0f) {
            uiServer.drawGlyph(globalPosition + glyph.position + glm::vec2(0.0f, verticalOffset),
                               glyph.size, color_,
                               glyph.uv, getDrawZIndex());
        }
    }
}
