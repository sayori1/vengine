#include "DepthPass.h"

#include "GeometryShaderSources.h"
#include "modules/renderer/DepthRenderer.h"
#include "modules/renderer/RenderContext.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace {
void requireResource(bool valid, const char* name) {
    if (valid) {
        return;
    }
    std::fprintf(stderr, "DepthPass: failed to create %s\n", name);
    std::abort();
}
}

struct DepthPass::Impl {
    DepthRenderer drawer{geometry_shaders::kDepthVertexSource,
                         geometry_shaders::kDepthFragmentSource, "depth pipeline"};
};

DepthPass::DepthPass() : impl_(std::make_unique<Impl>()) {}
DepthPass::~DepthPass() = default;

void DepthPass::execute(RenderContext& render, FrameContext& frame) {
    auto& target = render.depth;
    const int width = std::max(1, frame.viewport.width);
    const int height = std::max(1, frame.viewport.height);
    if (!target.image.id || target.width != width || target.height != height) {
        target.reset();
        sg_image_desc imageDesc{};
        imageDesc.width = width;
        imageDesc.height = height;
        imageDesc.pixel_format = SG_PIXELFORMAT_DEPTH;
        imageDesc.sample_count = 1;
        imageDesc.usage.depth_stencil_attachment = true;
        imageDesc.label = "scene depth";
        target.image = sg_make_image(&imageDesc);
        requireResource(sg_query_image_state(target.image) == SG_RESOURCESTATE_VALID, "depth image");
        sg_view_desc viewDesc{};
        viewDesc.depth_stencil_attachment.image = target.image;
        target.attachment = sg_make_view(&viewDesc);
        requireResource(sg_query_view_state(target.attachment) == SG_RESOURCESTATE_VALID, "depth attachment");
        viewDesc = {};
        viewDesc.texture.image = target.image;
        target.texture = sg_make_view(&viewDesc);
        requireResource(sg_query_view_state(target.texture) == SG_RESOURCESTATE_VALID, "depth texture view");
        target.width = width;
        target.height = height;
    }
    sg_pass pass{};
    pass.attachments.depth_stencil = target.attachment;
    pass.action.depth.load_action = SG_LOADACTION_CLEAR;
    pass.action.depth.store_action = SG_STOREACTION_STORE;
    pass.action.depth.clear_value = 1.0f;
    sg_begin_pass(&pass);
    if (frame.hasCamera) {
        impl_->drawer.draw(render, frame.viewProjection);
    }
    sg_end_pass();
    frame.depth = Depth{target.attachment, target.texture};
}
