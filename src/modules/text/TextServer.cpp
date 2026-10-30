#include "TextServer.h"

#include <hb-ft.h>
#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/unistr.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {

struct FaceHandle {
    FT_Face face = nullptr;
    ~FaceHandle() { if (face) FT_Done_Face(face); }
};

}

TextServer::TextServer() {
    const FT_Error error = FT_Init_FreeType(&library_);
    assert(error == 0);
    if (error != 0) std::abort();
}

TextServer::~TextServer() {
    if (library_) FT_Done_FreeType(library_);
}

TextServer::GlyphBitmap TextServer::rasterize(const Ref<Font>& font, uint32_t glyphId,
                                               int fontSize) {
    const uint64_t key = (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(font.get())) >> 4) ^
                         (static_cast<uint64_t>(fontSize) << 40) ^
                         (static_cast<uint64_t>(glyphId) << 8);
    if (const auto found = glyphs_.find(key); found != glyphs_.end()) return found->second;
    FaceHandle handle;
    if (font->getData().size() > static_cast<size_t>(std::numeric_limits<FT_Long>::max()) ||
        FT_New_Memory_Face(library_, font->getData().data(),
                           static_cast<FT_Long>(font->getData().size()), 0, &handle.face) != 0) {
        return {};
    }
    FT_Set_Pixel_Sizes(handle.face, 0, static_cast<FT_UInt>(fontSize));
    if (FT_Load_Glyph(handle.face, glyphId, FT_LOAD_RENDER) != 0) return {};
    const FT_Bitmap& bitmap = handle.face->glyph->bitmap;
    GlyphBitmap result;
    result.size = {static_cast<float>(bitmap.width), static_cast<float>(bitmap.rows)};
    result.bearing = {static_cast<float>(handle.face->glyph->bitmap_left),
                      static_cast<float>(handle.face->glyph->bitmap_top)};
    result.advance = static_cast<float>(handle.face->glyph->advance.x) / 64.0f;
    if (bitmap.width > 0 && bitmap.rows > 0) {
        const int width = static_cast<int>(bitmap.width);
        const int height = static_cast<int>(bitmap.rows);
        const int pitch = std::abs(bitmap.pitch);
        std::vector<uint8_t> alpha(static_cast<size_t>(width) * height);
        for (int y = 0; y < height; ++y) {
            const int sourceY = bitmap.pitch < 0 ? height - y - 1 : y;
            const uint8_t* row = bitmap.buffer + static_cast<size_t>(sourceY) * pitch;
            for (int x = 0; x < width; ++x) {
                uint8_t coverage = 0;
                if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
                    coverage = (row[x / 8] & (0x80u >> (x % 8))) ? 255 : 0;
                } else if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
                    coverage = row[x * 4 + 3];
                } else if (bitmap.pixel_mode == FT_PIXEL_MODE_LCD) {
                    const int pixel = x * 3;
                    coverage = static_cast<uint8_t>(
                        (static_cast<unsigned>(row[pixel]) + row[pixel + 1] + row[pixel + 2]) / 3);
                } else if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
                    coverage = bitmap.num_grays > 1
                        ? static_cast<uint8_t>(static_cast<unsigned>(row[x]) * 255 /
                                               (bitmap.num_grays - 1))
                        : row[x];
                }
                alpha[static_cast<size_t>(y) * width + x] = coverage;
            }
        }
        result.uv = atlas_.insert(width, height, alpha, width);
    }
    glyphs_.emplace(key, result);
    return result;
}

TextLayout TextServer::layout(const Ref<Font>& font, float fontSize, const std::string& text,
                              float maxWidth, bool wrap, float lineHeight, TextAlignment alignment) {
    TextLayout result;
    if (!font || text.empty() || fontSize <= 0.0f) return result;
    const std::string key = text + '\0' + std::to_string(reinterpret_cast<uintptr_t>(font.get())) +
        ':' + std::to_string(fontSize) + ':' + std::to_string(maxWidth) + ':' +
        std::to_string(lineHeight) + ':' + std::to_string(wrap) + ':' +
        std::to_string(static_cast<int>(alignment));
    if (const auto cached = layouts_.find(key); cached != layouts_.end()) return cached->second;
    const int pixelSize = std::max(1, static_cast<int>(std::round(fontSize)));
    FaceHandle handle;
    if (font->getData().size() > static_cast<size_t>(std::numeric_limits<FT_Long>::max()) ||
        FT_New_Memory_Face(library_, font->getData().data(),
                           static_cast<FT_Long>(font->getData().size()), 0, &handle.face) != 0) return result;
    FT_Set_Pixel_Sizes(handle.face, 0, static_cast<FT_UInt>(pixelSize));
    icu::UnicodeString unicode = icu::UnicodeString::fromUTF8(text);
    UErrorCode status = U_ZERO_ERROR;
    UBiDi* bidi = ubidi_openSized(unicode.length(), 0, &status);
    if (U_FAILURE(status) || !bidi) {
        return result;
    }
    UBreakIterator* breaker = ubrk_open(UBRK_LINE, "root", unicode.getBuffer(),
                                        unicode.length(), &status);
    const float lineStep = lineHeight > 0.0f ? lineHeight : static_cast<float>(pixelSize) * 1.2f;
    result.lineHeight = lineStep;
    float x = 0.0f;
    float y = static_cast<float>(pixelSize);
    int lineCount = 1;
    int start = 0;
    int end = unicode.length();
    while (start < end) {
        int paragraphEnd = unicode.indexOf(static_cast<UChar>('\n'), start);
        if (paragraphEnd < 0) paragraphEnd = end;
        status = U_ZERO_ERROR;
        ubidi_setPara(bidi, unicode.getBuffer() + start, paragraphEnd - start,
                      UBIDI_DEFAULT_LTR, nullptr, &status);
        const int runCount = U_SUCCESS(status) ? ubidi_countRuns(bidi, &status) : 0;
        for (int runIndex = 0; runIndex < runCount; ++runIndex) {
            int logicalStart = 0;
            int runLength = 0;
            const UBiDiDirection direction = ubidi_getVisualRun(bidi, runIndex, &logicalStart, &runLength);
            const icu::UnicodeString run = unicode.tempSubStringBetween(
                start + logicalStart, start + logicalStart + runLength);
            struct FontRun {
                int begin;
                int end;
                Ref<Font> font;
            };
            std::vector<FontRun> fontRuns;
            for (int offset = 0; offset < run.length();) {
                const UChar32 codepoint = run.char32At(offset);
                Ref<Font> selectedFont = font;
                if (FT_Get_Char_Index(handle.face, static_cast<FT_ULong>(codepoint)) == 0) {
                    for (const auto& candidate : font->getFallbacks()) {
                        FaceHandle candidateFace;
                        if (FT_New_Memory_Face(library_, candidate->getData().data(),
                                static_cast<FT_Long>(candidate->getData().size()), 0,
                                &candidateFace.face) == 0 &&
                            FT_Get_Char_Index(candidateFace.face, static_cast<FT_ULong>(codepoint)) != 0) {
                            selectedFont = candidate;
                            break;
                        }
                    }
                }
                const int next = offset + U16_LENGTH(codepoint);
                if (!fontRuns.empty() && fontRuns.back().font == selectedFont) {
                    fontRuns.back().end = next;
                } else {
                    fontRuns.push_back({offset, next, std::move(selectedFont)});
                }
                offset = next;
            }
            for (size_t segment = 0; segment < fontRuns.size(); ++segment) {
                const size_t index = direction == UBIDI_RTL
                    ? fontRuns.size() - segment - 1 : segment;
                const auto& fontRun = fontRuns[index];
                FaceHandle fallbackFace;
                FT_Face shapingFace = handle.face;
                if (fontRun.font != font) {
                    if (FT_New_Memory_Face(library_, fontRun.font->getData().data(),
                            static_cast<FT_Long>(fontRun.font->getData().size()), 0,
                            &fallbackFace.face) == 0) {
                        FT_Set_Pixel_Sizes(fallbackFace.face, 0, static_cast<FT_UInt>(pixelSize));
                        shapingFace = fallbackFace.face;
                    }
                }
                hb_font_t* runHbFont = hb_ft_font_create_referenced(shapingFace);
                const icu::UnicodeString segmentText =
                    run.tempSubStringBetween(fontRun.begin, fontRun.end);
                std::string utf8;
                segmentText.toUTF8String(utf8);
                std::vector<int> lineBreakOffsets;
                if (breaker) {
                    status = U_ZERO_ERROR;
                    ubrk_setText(breaker, segmentText.getBuffer(), segmentText.length(), &status);
                    if (U_SUCCESS(status)) {
                        for (int boundary = ubrk_first(breaker); boundary != UBRK_DONE;
                             boundary = ubrk_next(breaker)) {
                            std::string prefix;
                            segmentText.tempSubStringBetween(0, boundary).toUTF8String(prefix);
                            lineBreakOffsets.push_back(static_cast<int>(prefix.size()));
                        }
                    }
                }
                hb_buffer_t* buffer = hb_buffer_create();
                hb_buffer_add_utf8(buffer, utf8.data(), static_cast<int>(utf8.size()), 0,
                                   static_cast<int>(utf8.size()));
                hb_buffer_set_direction(buffer, direction == UBIDI_RTL ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
                hb_buffer_guess_segment_properties(buffer);
                hb_shape(runHbFont, buffer, nullptr, 0);
                unsigned glyphCount = 0;
                const hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buffer, &glyphCount);
                const hb_glyph_position_t* positions = hb_buffer_get_glyph_positions(buffer, &glyphCount);
                for (unsigned i = 0; i < glyphCount; ++i) {
                    GlyphBitmap glyph = rasterize(fontRun.font, info[i].codepoint, pixelSize);
                    const float advance = static_cast<float>(positions[i].x_advance) / 64.0f;
                    const bool lineBreak = std::find(lineBreakOffsets.begin(), lineBreakOffsets.end(),
                                                      static_cast<int>(info[i].cluster)) != lineBreakOffsets.end();
                    const bool clusterBoundary = i > 0 && info[i - 1].cluster != info[i].cluster;
                    if (wrap && (lineBreak || clusterBoundary) && maxWidth > 0.0f && x > 0.0f &&
                        x + advance > maxWidth) {
                        x = 0.0f;
                        y += lineStep;
                        ++lineCount;
                    }
                    result.glyphs.push_back({info[i].codepoint,
                        {x + static_cast<float>(positions[i].x_offset) / 64.0f + glyph.bearing.x,
                         y - static_cast<float>(positions[i].y_offset) / 64.0f - glyph.bearing.y},
                        glyph.size, glyph.uv, advance});
                    x += advance;
                }
                hb_buffer_destroy(buffer);
                hb_font_destroy(runHbFont);
            }
        }
        result.size.x = std::max(result.size.x, x);
        if (paragraphEnd == end) break;
        x = 0.0f;
        y += lineStep;
        ++lineCount;
        start = paragraphEnd + 1;
    }
    result.size.y = static_cast<float>(lineCount) * lineStep;
    if (alignment != TextAlignment::Left && maxWidth > 0.0f) {
        std::vector<float> widths(static_cast<size_t>(lineCount), 0.0f);
        for (const auto& glyph : result.glyphs) {
            const size_t line = std::min(static_cast<size_t>(std::max(0.0f, glyph.position.y) / lineStep),
                                         widths.size() - 1);
            widths[line] = std::max(widths[line], glyph.position.x + glyph.size.x);
        }
        for (auto& glyph : result.glyphs) {
            const size_t line = std::min(static_cast<size_t>(std::max(0.0f, glyph.position.y) / lineStep),
                                         widths.size() - 1);
            const float offset = alignment == TextAlignment::Center
                ? (maxWidth - widths[line]) * 0.5f : maxWidth - widths[line];
            glyph.position.x += std::max(0.0f, offset);
        }
    }
    if (breaker) ubrk_close(breaker);
    ubidi_close(bidi);
    if (layouts_.size() >= 128) layouts_.clear();
    layouts_.emplace(key, result);
    return result;
}

const std::vector<uint8_t>& TextServer::getAtlasPixels() const { return *atlas_.getPixels(); }
std::shared_ptr<const std::vector<uint8_t>> TextServer::getSharedAtlasPixels() const { return atlas_.getPixels(); }
uint64_t TextServer::getAtlasVersion() const { return atlas_.getVersion(); }
