#pragma once

#include <glm/vec4.hpp>

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

class GlyphAtlas {
public:
    static constexpr int kSize = 1024;

    GlyphAtlas();
    glm::vec4 insert(int width, int height, std::span<const uint8_t> alpha, int rowPitch);
    std::shared_ptr<const std::vector<uint8_t>> getPixels() const;
    uint64_t getVersion() const;

private:
    std::shared_ptr<std::vector<uint8_t>> pixels_;
    int x_ = 1;
    int y_ = 0;
    int row_height_ = 0;
    uint64_t version_ = 1;
};
