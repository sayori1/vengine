#include "Texture2D.h"

#include <limits>

Texture2D::Texture2D(ColorSpace colorSpace) : color_space_(colorSpace) {}

Ref<Texture2D> Texture2D::create(int width, int height, std::span<const uint8_t> pixels, ColorSpace colorSpace) {
    if (!sg_isvalid() || width <= 0 || height <= 0 ||
        (colorSpace != ColorSpace::Srgb && colorSpace != ColorSpace::Linear)) {
        return nullptr;
    }
    const size_t rowSize = static_cast<size_t>(width) * 4;
    if (static_cast<size_t>(height) > std::numeric_limits<size_t>::max() / rowSize ||
        pixels.size() != rowSize * static_cast<size_t>(height)) {
        return nullptr;
    }
    auto texture = Ref<Texture2D>(new Texture2D(colorSpace));
    sg_image_desc imageDesc{};
    imageDesc.width = width;
    imageDesc.height = height;
    imageDesc.pixel_format = colorSpace == ColorSpace::Srgb ? SG_PIXELFORMAT_SRGB8A8 : SG_PIXELFORMAT_RGBA8;
    imageDesc.data = {pixels.data(), pixels.size()};
    texture->image_ = sg_make_image(&imageDesc);
    if (sg_query_image_state(texture->image_) != SG_RESOURCESTATE_VALID) {
        return nullptr;
    }
    sg_view_desc viewDesc{};
    viewDesc.texture.image = texture->image_;
    texture->view_ = sg_make_view(&viewDesc);
    if (sg_query_view_state(texture->view_) != SG_RESOURCESTATE_VALID) {
        return nullptr;
    }
    sg_sampler_desc samplerDesc{};
    samplerDesc.min_filter = SG_FILTER_LINEAR;
    samplerDesc.mag_filter = SG_FILTER_LINEAR;
    samplerDesc.wrap_u = SG_WRAP_REPEAT;
    samplerDesc.wrap_v = SG_WRAP_REPEAT;
    texture->sampler_ = sg_make_sampler(&samplerDesc);
    if (sg_query_sampler_state(texture->sampler_) != SG_RESOURCESTATE_VALID) {
        return nullptr;
    }
    return texture;
}

Texture2D::~Texture2D() {
    if (sampler_.id != 0) {
        sg_destroy_sampler(sampler_);
    }
    if (view_.id != 0) {
        sg_destroy_view(view_);
    }
    if (image_.id != 0) {
        sg_destroy_image(image_);
    }
}

sg_view Texture2D::getView() const { return view_; }
sg_sampler Texture2D::getSampler() const { return sampler_; }
Texture2D::ColorSpace Texture2D::getColorSpace() const { return color_space_; }
