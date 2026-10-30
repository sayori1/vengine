#include "UIPass.h"

#include "GeometryShaderSources.h"
#include "modules/renderer/FrameContext.h"
#include "modules/renderer/RenderContext.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

struct UIVertex {
    glm::vec2 position;
    glm::vec4 color;
    glm::vec2 uv;
    float textured = 0.0f;
};

}

struct UIPass::Impl {
    sg_shader shader{};
    sg_pipeline pipeline{};
    sg_buffer vertices{};
    sg_buffer indices{};
    sg_image atlas{};
    sg_view atlas_view{};
    sg_sampler atlas_sampler{};
    sg_buffer atlas_staging{};
    uint64_t atlas_version = 0;
    size_t capacity = 0;
    size_t indexCapacity = 0;
    sg_pixel_format colorFormat = SG_PIXELFORMAT_NONE;
    sg_pixel_format depthFormat = SG_PIXELFORMAT_NONE;
    int sampleCount = 0;

    ~Impl() {
        if (indices.id) sg_destroy_buffer(indices);
        if (vertices.id) sg_destroy_buffer(vertices);
        if (atlas_sampler.id) sg_destroy_sampler(atlas_sampler);
        if (atlas_view.id) sg_destroy_view(atlas_view);
        if (atlas.id) sg_destroy_image(atlas);
        if (atlas_staging.id) sg_destroy_buffer(atlas_staging);
        if (pipeline.id) sg_destroy_pipeline(pipeline);
        if (shader.id) sg_destroy_shader(shader);
    }

    void initialize(const sg_swapchain& swapchain) {
        if (pipeline.id && colorFormat == swapchain.color_format &&
            depthFormat == swapchain.depth_format && sampleCount == swapchain.sample_count) return;
        if (pipeline.id) sg_destroy_pipeline(pipeline);
        if (shader.id) sg_destroy_shader(shader);
        sg_shader_desc shaderDesc{};
        shaderDesc.vertex_func.source = ui_shaders::kVertexSource;
        shaderDesc.fragment_func.source = ui_shaders::kFragmentSource;
        shaderDesc.attrs[0].glsl_name = "a_position";
        shaderDesc.attrs[1].glsl_name = "a_color";
        shaderDesc.attrs[2].glsl_name = "a_uv";
        shaderDesc.attrs[3].glsl_name = "a_textured";
        auto& texture = shaderDesc.views[0].texture;
        texture.stage = SG_SHADERSTAGE_FRAGMENT;
        texture.image_type = SG_IMAGETYPE_2D;
        texture.sample_type = SG_IMAGESAMPLETYPE_FLOAT;
        shaderDesc.samplers[0].stage = SG_SHADERSTAGE_FRAGMENT;
        shaderDesc.samplers[0].sampler_type = SG_SAMPLERTYPE_FILTERING;
        auto& pair = shaderDesc.texture_sampler_pairs[0];
        pair.stage = SG_SHADERSTAGE_FRAGMENT;
        pair.view_slot = 0;
        pair.sampler_slot = 0;
        pair.glsl_name = "u_atlas";
        shader = sg_make_shader(&shaderDesc);
        if (sg_query_shader_state(shader) != SG_RESOURCESTATE_VALID) {
            std::fprintf(stderr, "UIPass: UI shader creation failed\n");
            std::abort();
        }
        sg_pipeline_desc pipelineDesc{};
        pipelineDesc.shader = shader;
        pipelineDesc.colors[0].pixel_format = swapchain.color_format;
        pipelineDesc.depth.pixel_format = swapchain.depth_format;
        pipelineDesc.sample_count = swapchain.sample_count;
        pipelineDesc.layout.buffers[0].stride = sizeof(UIVertex);
        pipelineDesc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT2;
        pipelineDesc.layout.attrs[0].offset = offsetof(UIVertex, position);
        pipelineDesc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT4;
        pipelineDesc.layout.attrs[1].offset = offsetof(UIVertex, color);
        pipelineDesc.layout.attrs[2].format = SG_VERTEXFORMAT_FLOAT2;
        pipelineDesc.layout.attrs[2].offset = offsetof(UIVertex, uv);
        pipelineDesc.layout.attrs[3].format = SG_VERTEXFORMAT_FLOAT;
        pipelineDesc.layout.attrs[3].offset = offsetof(UIVertex, textured);
        pipelineDesc.index_type = SG_INDEXTYPE_UINT32;
        pipelineDesc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
        pipelineDesc.colors[0].blend.enabled = true;
        pipelineDesc.colors[0].blend.src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA;
        pipelineDesc.colors[0].blend.dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        pipelineDesc.colors[0].blend.op_rgb = SG_BLENDOP_ADD;
        pipelineDesc.colors[0].blend.src_factor_alpha = SG_BLENDFACTOR_ONE;
        pipelineDesc.colors[0].blend.dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        pipelineDesc.colors[0].blend.op_alpha = SG_BLENDOP_ADD;
        pipelineDesc.depth.compare = SG_COMPAREFUNC_ALWAYS;
        pipelineDesc.depth.write_enabled = false;
        pipelineDesc.cull_mode = SG_CULLMODE_NONE;
        pipeline = sg_make_pipeline(&pipelineDesc);
        if (sg_query_pipeline_state(pipeline) != SG_RESOURCESTATE_VALID) {
            std::fprintf(stderr, "UIPass: UI pipeline creation failed\n");
            std::abort();
        }
        colorFormat = swapchain.color_format;
        depthFormat = swapchain.depth_format;
        sampleCount = swapchain.sample_count;
        if (!atlas.id) {
            sg_image_desc imageDesc{};
            imageDesc.width = 1024;
            imageDesc.height = 1024;
            imageDesc.pixel_format = SG_PIXELFORMAT_RGBA8;
            imageDesc.usage.copy_dst = true;
            atlas = sg_make_image(&imageDesc);
            sg_view_desc viewDesc{};
            viewDesc.texture.image = atlas;
            atlas_view = sg_make_view(&viewDesc);
            sg_sampler_desc samplerDesc{};
            samplerDesc.min_filter = SG_FILTER_NEAREST;
            samplerDesc.mag_filter = SG_FILTER_NEAREST;
            samplerDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
            samplerDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
            atlas_sampler = sg_make_sampler(&samplerDesc);
            sg_buffer_desc stagingDesc{};
            stagingDesc.size = 1024 * 1024 * 4;
            stagingDesc.usage.staging_buffer = true;
            stagingDesc.usage.write_transient = true;
            stagingDesc.usage.copy_src = true;
            atlas_staging = sg_make_buffer(&stagingDesc);
        }
    }
};

UIPass::UIPass() : impl_(std::make_unique<Impl>()) {}
UIPass::~UIPass() = default;

void UIPass::execute(RenderContext&, FrameContext& frame) {
    if (frame.viewport.width <= 0 || frame.viewport.height <= 0 || frame.uiCommands.empty()) return;
    std::stable_sort(frame.uiCommands.begin(), frame.uiCommands.end(),
                     [](const auto& left, const auto& right) { return left.zIndex < right.zIndex; });
    std::vector<UIVertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(frame.uiCommands.size() * 4);
    indices.reserve(frame.uiCommands.size() * 6);
    for (const auto& command : frame.uiCommands) {
        const uint32_t base = static_cast<uint32_t>(vertices.size());
        const auto p = command.position;
        const auto s = command.size;
        const auto ndc = [&frame](const glm::vec2& point) {
            return glm::vec2(point.x * 2.0f / frame.viewport.width - 1.0f,
                             1.0f - point.y * 2.0f / frame.viewport.height);
        };
        const float x0 = command.textured ? command.uv.x : 0.0001f;
        const float y0 = command.textured ? command.uv.y : 0.0001f;
        const float x1 = command.textured ? command.uv.z : 0.0002f;
        const float y1 = command.textured ? command.uv.w : 0.0002f;
        const float textured = command.textured ? 1.0f : 0.0f;
        vertices.insert(vertices.end(), {
            {ndc(p),command.color,{x0,y0},textured},
            {ndc(p + glm::vec2(s.x, 0.0f)),command.color,{x1,y0},textured},
            {ndc(p + s),command.color,{x1,y1},textured},
            {ndc(p + glm::vec2(0.0f, s.y)),command.color,{x0,y1},textured}});
        indices.insert(indices.end(), {base,base+1,base+2,base,base+2,base+3});
    }
    impl_->initialize(frame.swapchain);
    const auto ensureBuffer = [](sg_buffer& buffer, size_t size, bool vertex) {
        sg_buffer_desc desc{};
        desc.size = size;
        desc.usage.vertex_buffer = vertex;
        desc.usage.index_buffer = !vertex;
        desc.usage.write_transient = true;
        if (buffer.id) sg_destroy_buffer(buffer);
        buffer = sg_make_buffer(&desc);
    };
    const size_t vertexBytes = vertices.size() * sizeof(UIVertex);
    const size_t indexBytes = indices.size() * sizeof(uint32_t);
    if (vertexBytes > impl_->capacity) {
        impl_->capacity = std::max(vertexBytes, impl_->capacity * 2);
        ensureBuffer(impl_->vertices, impl_->capacity, true);
    }
    if (indexBytes > impl_->indexCapacity) {
        impl_->indexCapacity = std::max(indexBytes, impl_->indexCapacity * 2);
        ensureBuffer(impl_->indices, impl_->indexCapacity, false);
    }
    sg_write_buffer_desc vertexWrite{};
    vertexWrite.src.data = {vertices.data(), vertexBytes};
    vertexWrite.dst.buffer = impl_->vertices;
    vertexWrite.size = vertexBytes;
    sg_write_buffer_transient(&vertexWrite);
    sg_write_buffer_desc indexWrite{};
    indexWrite.src.data = {indices.data(), indexBytes};
    indexWrite.dst.buffer = impl_->indices;
    indexWrite.size = indexBytes;
    sg_write_buffer_transient(&indexWrite);
    if (frame.glyphAtlas && frame.glyphAtlasVersion != impl_->atlas_version &&
        frame.glyphAtlas->size() == 1024 * 1024 * 4) {
        sg_write_buffer_desc atlasWrite{};
        atlasWrite.src.data = {frame.glyphAtlas->data(), frame.glyphAtlas->size()};
        atlasWrite.dst.buffer = impl_->atlas_staging;
        atlasWrite.size = frame.glyphAtlas->size();
        sg_write_buffer_transient(&atlasWrite);
        sg_copy_buffer_to_image_desc copy{};
        copy.src.buffer = impl_->atlas_staging;
        copy.dst.image = impl_->atlas;
        copy.size = {1024, 1024, 1};
        sg_copy_buffer_to_image(&copy);
        impl_->atlas_version = frame.glyphAtlasVersion;
    }
    if (sg_query_buffer_state(impl_->vertices) != SG_RESOURCESTATE_VALID ||
        sg_query_buffer_state(impl_->indices) != SG_RESOURCESTATE_VALID) {
        std::fprintf(stderr, "UIPass: transient vertex/index buffer invalid\n");
        std::abort();
    }
    sg_pass pass{};
    pass.swapchain = frame.swapchain;
    pass.action.colors[0].load_action = SG_LOADACTION_LOAD;
    pass.action.colors[0].store_action = SG_STOREACTION_STORE;
    pass.action.depth.load_action = SG_LOADACTION_DONTCARE;
    pass.action.depth.store_action = SG_STOREACTION_DONTCARE;
    pass.action.stencil.load_action = SG_LOADACTION_DONTCARE;
    pass.action.stencil.store_action = SG_STOREACTION_DONTCARE;
    sg_begin_pass(&pass);
    sg_apply_pipeline(impl_->pipeline);
    sg_bindings bindings{};
    bindings.vertex_buffers[0] = impl_->vertices;
    bindings.index_buffer = impl_->indices;
    bindings.views[0] = impl_->atlas_view;
    bindings.samplers[0] = impl_->atlas_sampler;
    sg_apply_bindings(&bindings);
    sg_draw(0, static_cast<int>(indices.size()), 1);
    sg_end_pass();
    frame.uiCommands.clear();
}
