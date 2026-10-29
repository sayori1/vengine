#include "RenderContext.h"

namespace {
void destroy(sg_view& view) {
    if (view.id) sg_destroy_view(view);
    view = {};
}
void destroy(sg_buffer& buffer) {
    if (buffer.id) sg_destroy_buffer(buffer);
    buffer = {};
}
void destroy(sg_image& image) {
    if (image.id) sg_destroy_image(image);
    image = {};
}
}

GPUSurface::~GPUSurface() {
    destroy(vertexBuffer);
    destroy(indexBuffer);
}

DepthTarget::~DepthTarget() { reset(); }
void DepthTarget::reset() {
    destroy(attachment);
    destroy(texture);
    destroy(image);
    width = height = 0;
}

ColorTarget::~ColorTarget() { reset(); }
void ColorTarget::reset() {
    destroy(attachment);
    destroy(texture);
    destroy(image);
    width = height = 0;
}

LightingResources::~LightingResources() {
    resetClusters();
    resetShadows();
}

void LightingResources::resetClusters() {
    destroy(lightView);
    destroy(clusterView);
    destroy(indexView);
    destroy(lightBuffer);
    destroy(clusterBuffer);
    destroy(indexBuffer);
}

void LightingResources::resetShadows() {
    destroy(shadowView);
    destroy(shadowBuffer);
    destroy(shadowAttachment);
    destroy(shadowTexture);
    if (shadowSampler.id) sg_destroy_sampler(shadowSampler);
    shadowSampler = {};
    destroy(shadowAtlas);
}
