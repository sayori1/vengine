#include "GlyphAtlas.h"

#include <algorithm>
#include <cstdlib>

GlyphAtlas::GlyphAtlas()
    : pixels_(std::make_shared<std::vector<uint8_t>>(kSize * kSize * 4, 0)) {
    (*pixels_)[0] = 255;
    (*pixels_)[1] = 255;
    (*pixels_)[2] = 255;
    (*pixels_)[3] = 255;
}

glm::vec4 GlyphAtlas::insert(int width, int height, std::span<const uint8_t> alpha,
                             int rowPitch) {
    if (width <= 0 || height <= 0) return glm::vec4(0.0f);
    if (alpha.size() < static_cast<size_t>(std::abs(rowPitch)) * height) return glm::vec4(0.0f);
    if (x_ + width + 1 >= kSize) {
        x_ = 1;
        y_ += row_height_ + 1;
        row_height_ = 0;
    }
    if (y_ + height + 1 >= kSize) return glm::vec4(0.0f);
    const int pitch = std::abs(rowPitch);
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            const size_t destination =
                (static_cast<size_t>(y_ + row) * kSize + x_ + column) * 4;
            const size_t source = static_cast<size_t>(row) * pitch + column;
            (*pixels_)[destination] = 255;
            (*pixels_)[destination + 1] = 255;
            (*pixels_)[destination + 2] = 255;
            (*pixels_)[destination + 3] = alpha[source];
        }
    }
    const glm::vec4 uv(static_cast<float>(x_) / kSize, static_cast<float>(y_) / kSize,
                       static_cast<float>(x_ + width) / kSize,
                       static_cast<float>(y_ + height) / kSize);
    x_ += width + 1;
    row_height_ = std::max(row_height_, height);
    ++version_;
    return uv;
}

std::shared_ptr<const std::vector<uint8_t>> GlyphAtlas::getPixels() const { return pixels_; }
uint64_t GlyphAtlas::getVersion() const { return version_; }
