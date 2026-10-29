#pragma once

#include "modules/resource/Resource.h"
#include "sokol_gfx.h"

#include <span>

class Texture2D : public Resource {
public:
    enum class ColorSpace { Srgb, Linear };

    static Ref<Texture2D> create(int width, int height, std::span<const uint8_t> pixels, ColorSpace colorSpace);
    ~Texture2D() override;

    sg_view getView() const;
    sg_sampler getSampler() const;
    ColorSpace getColorSpace() const;

private:
    explicit Texture2D(ColorSpace colorSpace);

    sg_image image_{};
    sg_view view_{};
    sg_sampler sampler_{};
    ColorSpace color_space_;
};
