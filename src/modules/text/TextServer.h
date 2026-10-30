#pragma once

#include "Font.h"
#include "GlyphAtlas.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct TextGlyph {
    uint32_t id = 0;
    glm::vec2 position{0.0f};
    glm::vec2 size{0.0f};
    glm::vec4 uv{0.0f};
    float advance = 0.0f;
};

enum class TextAlignment {
    Left,
    Center,
    Right
};

enum class VerticalTextAlignment {
    Top,
    Center,
    Bottom
};

struct TextLayout {
    std::vector<TextGlyph> glyphs;
    glm::vec2 size{0.0f};
    float lineHeight = 0.0f;
};

class TextServer {
public:
    TextServer();
    ~TextServer();
    TextServer(const TextServer&) = delete;
    TextServer& operator=(const TextServer&) = delete;

    TextLayout layout(const Ref<Font>& font, float fontSize, const std::string& text,
                      float maxWidth, bool wrap, float lineHeight, TextAlignment alignment);
    const std::vector<uint8_t>& getAtlasPixels() const;
    std::shared_ptr<const std::vector<uint8_t>> getSharedAtlasPixels() const;
    uint64_t getAtlasVersion() const;

private:
    struct GlyphBitmap {
        glm::vec2 size{0.0f};
        glm::vec2 bearing{0.0f};
        glm::vec4 uv{0.0f};
        float advance = 0.0f;
    };

    GlyphBitmap rasterize(const Ref<Font>& font, uint32_t glyphId, int fontSize);
    FT_Library library_ = nullptr;
    GlyphAtlas atlas_;
    std::unordered_map<uint64_t, GlyphBitmap> glyphs_;
    std::unordered_map<std::string, TextLayout> layouts_;
};
