#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include "modules/text/Font.h"
#include <vector>

class Control;
class Node;
class TextServer;
class Font;
struct TextLayout;

class UIServer {
public:
    void registerRoot(Control& control);
    void unregisterRoot(Control& control);
    void update(const glm::vec2& viewportSize);
    void drawRect(const glm::vec2& position, const glm::vec2& size,
                  const glm::vec4& color, int zIndex);
    void setTextServer(TextServer& textServer, Ref<Font> defaultFont);
    TextServer& getTextServer();
    const Ref<Font>& getDefaultFont() const;
    void drawGlyph(const glm::vec2& position, const glm::vec2& size,
                   const glm::vec4& color, const glm::vec4& uv, int zIndex);

private:
    void layout(Control& control, const glm::vec2& parentPosition,
                const glm::vec2& availableSize);
    void draw(Control& control, int parentZ);
    void layoutDescendants(Node& node, const glm::vec2& parentPosition,
                           const glm::vec2& availableSize);
    void drawDescendants(Node& node, int parentZ);
    std::vector<Control*> roots_;
    TextServer* text_server_ = nullptr;
    Ref<Font> default_font_;
};
