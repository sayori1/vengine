#include "GeometryPass.h"

#include "GeometryShaderSources.h"
#include "modules/renderer/Material.h"
#include "modules/renderer/Mesh.h"
#include "modules/renderer/RenderContext.h"
#include "modules/renderer/Shader.h"
#include "modules/renderer/StandardMaterial3D.h"
#include "modules/renderer/Texture2D.h"
#include "modules/renderer/Vertex.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <variant>

namespace {

void require(bool valid, const char* message) {
    if (!valid) {
        std::fprintf(stderr, "GeometryPass: %s\n", message);
        std::abort();
    }
}

struct GPUShader {
    std::weak_ptr<Shader> resource;
    uint64_t version = 0;
    sg_shader shader{};
    sg_pipeline pipeline{};

    GPUShader() = default;
    GPUShader(const GPUShader&) = delete;
    GPUShader& operator=(const GPUShader&) = delete;

    ~GPUShader() {
        destroy();
    }

    void destroy() {
        if (pipeline.id) {
            sg_destroy_pipeline(pipeline);
            pipeline = {};
        }
        if (shader.id) {
            sg_destroy_shader(shader);
            shader = {};
        }
        version = 0;
    }
};

struct VertexUniforms {
    glm::mat4 mvp{1.0f};
};

struct FragmentUniforms {
    glm::vec4 color{1.0f};
};

struct PbrVertexUniforms {
    glm::mat4 mvp{1.0f};
    glm::mat4 model{1.0f};
    glm::mat4 normalMatrix{1.0f};
};

struct PbrFragmentUniforms {
    glm::mat4 view{1.0f};
    glm::vec4 baseColor{1.0f};
    glm::vec4 material{0.0f};
    glm::vec4 emission{0.0f};
    glm::vec4 camera{0.0f};
    glm::vec4 ambient{0.0f};
    glm::vec4 screen{1.0f};
    glm::vec4 grid{1.0f};
    glm::vec4 zParameters{0.0f};
    glm::vec4 shadowTexel{0.0f};
};

static_assert(sizeof(VertexUniforms) == 64);
static_assert(sizeof(FragmentUniforms) == 16);
static_assert(sizeof(PbrVertexUniforms) == 192);
static_assert(sizeof(PbrFragmentUniforms) == 208);
static_assert(offsetof(PbrFragmentUniforms, baseColor) == 64);
static_assert(offsetof(PbrFragmentUniforms, shadowTexel) == 192);

void setUniformBlock(sg_shader_uniform_block& block, sg_shader_stage stage, uint32_t size) {
    block.stage = stage;
    block.size = size;
    block.layout = SG_UNIFORMLAYOUT_STD140;
}

void setUniform(sg_shader_uniform_block& block, int index, const char* name, sg_uniform_type type) {
    block.glsl_uniforms[index].glsl_name = name;
    block.glsl_uniforms[index].type = type;
    block.glsl_uniforms[index].array_count = 1;
}

void setTexture(sg_shader_desc& desc, int pair, int view, int sampler, const char* name,
                sg_image_sample_type sampleType = SG_IMAGESAMPLETYPE_FLOAT) {
    auto& texture = desc.views[view].texture;
    texture.stage = SG_SHADERSTAGE_FRAGMENT;
    texture.image_type = SG_IMAGETYPE_2D;
    texture.sample_type = sampleType;
    desc.samplers[sampler].stage = SG_SHADERSTAGE_FRAGMENT;
    desc.samplers[sampler].sampler_type = SG_SAMPLERTYPE_FILTERING;
    auto& textureSampler = desc.texture_sampler_pairs[pair];
    textureSampler.stage = SG_SHADERSTAGE_FRAGMENT;
    textureSampler.view_slot = static_cast<uint8_t>(view);
    textureSampler.sampler_slot = static_cast<uint8_t>(sampler);
    textureSampler.glsl_name = name;
}

sg_pipeline_desc geometryPipeline(sg_shader shader, bool pbr) {
    sg_pipeline_desc desc{};
    desc.shader = shader;
    desc.layout.buffers[0].stride = sizeof(Vertex);
    desc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
    desc.layout.attrs[0].offset = offsetof(Vertex, position);
    desc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT4;
    desc.layout.attrs[1].offset = offsetof(Vertex, color);
    if (pbr) {
        desc.layout.attrs[2].format = SG_VERTEXFORMAT_FLOAT3;
        desc.layout.attrs[2].offset = offsetof(Vertex, normal);
        desc.layout.attrs[3].format = SG_VERTEXFORMAT_FLOAT2;
        desc.layout.attrs[3].offset = offsetof(Vertex, uv);
        desc.layout.attrs[4].format = SG_VERTEXFORMAT_FLOAT4;
        desc.layout.attrs[4].offset = offsetof(Vertex, tangent);
    }
    desc.index_type = SG_INDEXTYPE_UINT32;
    desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    desc.colors[0].pixel_format = SG_PIXELFORMAT_RGBA16F;
    desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH;
    desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    desc.depth.write_enabled = false;
    desc.sample_count = 1;
    desc.cull_mode = SG_CULLMODE_NONE;
    desc.label = pbr ? "clustered PBR HDR pipeline" : "legacy material HDR pipeline";
    return desc;
}

void createGPUShader(GPUShader& gpu, const Shader& resource) {
    sg_shader_desc desc{};
    desc.vertex_func.source = resource.getVertexSource().c_str();
    desc.fragment_func.source = resource.getFragmentSource().c_str();
    desc.attrs[0].glsl_name = "a_position";
    desc.attrs[1].glsl_name = "a_color";
    setUniformBlock(desc.uniform_blocks[0], SG_SHADERSTAGE_VERTEX, sizeof(VertexUniforms));
    setUniform(desc.uniform_blocks[0], 0, "u_mvp", SG_UNIFORMTYPE_MAT4);
    setUniformBlock(desc.uniform_blocks[1], SG_SHADERSTAGE_FRAGMENT, sizeof(FragmentUniforms));
    setUniform(desc.uniform_blocks[1], 0, "u_color", SG_UNIFORMTYPE_FLOAT4);
    desc.label = "legacy material shader";
    gpu.shader = sg_make_shader(&desc);
    require(sg_query_shader_state(gpu.shader) == SG_RESOURCESTATE_VALID,
            "legacy shader creation failed; check custom vertex/fragment sources and u_mvp/u_color reflection");
    const auto pipelineDesc = geometryPipeline(gpu.shader, false);
    gpu.pipeline = sg_make_pipeline(&pipelineDesc);
    require(sg_query_pipeline_state(gpu.pipeline) == SG_RESOURCESTATE_VALID,
            "legacy HDR pipeline creation failed; RGBA16F color and single-sample DEPTH support required");
    gpu.version = resource.getVersion();
}

const GPUSurface* getSurface(RenderContext& context, RID rid, size_t index) {
    const auto* mesh = context.meshes.get(rid);
    if (!mesh || index >= mesh->surfaces.size() || !mesh->surfaces[index]) {
        return nullptr;
    }
    const auto* surface = mesh->surfaces[index].get();
    if (surface->indexCount <= 0 ||
        sg_query_buffer_state(surface->vertexBuffer) != SG_RESOURCESTATE_VALID ||
        sg_query_buffer_state(surface->indexBuffer) != SG_RESOURCESTATE_VALID) {
        return nullptr;
    }
    return surface;
}

bool finiteMatrix(const glm::mat4& matrix) {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (!std::isfinite(matrix[column][row])) {
                return false;
            }
        }
    }
    return true;
}

bool finiteVector(const glm::vec4& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) &&
           std::isfinite(value.z) && std::isfinite(value.w);
}

void applyUniforms(int slot, const void* data, size_t size) {
    const sg_range range{data, size};
    sg_apply_uniforms(slot, &range);
}

}

struct GeometryPass::Impl {
    std::unordered_map<const Shader*, std::unique_ptr<GPUShader>> shaders;
    std::unordered_map<const Shader*, std::unique_ptr<GPUShader>> pbrShaders;
    GPUShader tonemap;
    std::array<std::shared_ptr<Texture2D>, 5> defaults;
    sg_sampler hdrSampler{};
    sg_pixel_format outputColor = SG_PIXELFORMAT_NONE;
    sg_pixel_format outputDepth = SG_PIXELFORMAT_NONE;
    int outputSamples = 0;

    ~Impl() {
        if (hdrSampler.id) {
            sg_destroy_sampler(hdrSampler);
        }
    }

    GPUShader* getShader(const std::shared_ptr<Shader>& shader, bool pbrContract = false) {
        if (!shader) {
            return nullptr;
        }
        auto& cache = pbrContract ? pbrShaders : shaders;
        for (auto it = cache.begin(); it != cache.end();) {
            if (it->second->resource.expired()) {
                it = cache.erase(it);
            } else {
                ++it;
            }
        }
        auto& gpu = cache[shader.get()];
        if (!gpu) {
            gpu = std::make_unique<GPUShader>();
        }
        gpu->resource = shader;
        if (gpu->version != shader->getVersion() || !gpu->pipeline.id) {
            gpu->destroy();
            if (pbrContract) {
                createPbrShader(*gpu, *shader);
            } else {
                createGPUShader(*gpu, *shader);
            }
        }
        return gpu.get();
    }

    void createPbrShader(GPUShader& pbr, const Shader& resource) {
        require(sg_query_limits().max_storage_buffer_bindings_per_stage >= 4,
                "clustered PBR requires four fragment storage buffers; select a capable OpenGL 4.3+ device");
        sg_shader_desc desc{};
        desc.vertex_func.source = resource.getVertexSource().c_str();
        desc.fragment_func.source = resource.getFragmentSource().c_str();
        const char* attributes[] = {"a_position", "a_color", "a_normal", "a_uv", "a_tangent"};
        for (int i = 0; i < 5; ++i) {
            desc.attrs[i].glsl_name = attributes[i];
        }
        auto& vertex = desc.uniform_blocks[0];
        setUniformBlock(vertex, SG_SHADERSTAGE_VERTEX, sizeof(PbrVertexUniforms));
        const char* matrices[] = {"u_mvp", "u_model", "u_normal_matrix"};
        for (int i = 0; i < 3; ++i) {
            setUniform(vertex, i, matrices[i], SG_UNIFORMTYPE_MAT4);
        }
        auto& fragment = desc.uniform_blocks[1];
        setUniformBlock(fragment, SG_SHADERSTAGE_FRAGMENT, sizeof(PbrFragmentUniforms));
        setUniform(fragment, 0, "u_view", SG_UNIFORMTYPE_MAT4);
        const char* parameters[] = {"u_base_color", "u_material", "u_emission", "u_camera",
                                    "u_ambient", "u_screen", "u_grid", "u_z_parameters", "u_shadow_texel"};
        for (int i = 0; i < 9; ++i) {
            setUniform(fragment, i + 1, parameters[i], SG_UNIFORMTYPE_FLOAT4);
        }
        const char* textures[] = {"u_albedo_texture", "u_normal_texture", "u_metallic_roughness_texture",
                                  "u_ao_texture", "u_emissive_texture"};
        for (int i = 0; i < 5; ++i) {
            setTexture(desc, i, i, i, textures[i]);
        }
        for (int i = 0; i < 4; ++i) {
            auto& storage = desc.views[5 + i].storage_buffer;
            storage.stage = SG_SHADERSTAGE_FRAGMENT;
            storage.readonly = true;
            storage.glsl_binding_n = static_cast<uint8_t>(i);
        }
        setTexture(desc, 5, 9, 5, "u_shadow_atlas", SG_IMAGESAMPLETYPE_UNFILTERABLE_FLOAT);
        desc.samplers[5].sampler_type = SG_SAMPLERTYPE_NONFILTERING;
        desc.label = "clustered PBR shader";
        pbr.shader = sg_make_shader(&desc);
        require(sg_query_shader_state(pbr.shader) == SG_RESOURCESTATE_VALID,
                "PBR shader creation failed; check material Shader sources, GLSL 4.3, and the PBR uniform/texture/storage-buffer contract");
        const auto pipelineDesc = geometryPipeline(pbr.shader, true);
        pbr.pipeline = sg_make_pipeline(&pipelineDesc);
        require(sg_query_pipeline_state(pbr.pipeline) == SG_RESOURCESTATE_VALID,
                "PBR HDR pipeline creation failed; RGBA16F color and single-sample DEPTH support required");
        pbr.version = resource.getVersion();
    }

    void initializeDefaults() {
        if (defaults[0]) {
            return;
        }
        const std::array<uint8_t, 4> white{255, 255, 255, 255};
        const std::array<uint8_t, 4> flat{128, 128, 255, 255};
        defaults[0] = Texture2D::create(1, 1, white, Texture2D::ColorSpace::Srgb);
        defaults[1] = Texture2D::create(1, 1, flat, Texture2D::ColorSpace::Linear);
        defaults[2] = Texture2D::create(1, 1, white, Texture2D::ColorSpace::Linear);
        defaults[3] = defaults[2];
        defaults[4] = defaults[0];
        for (const auto& texture : defaults) {
            require(static_cast<bool>(texture), "material default texture creation failed; check Sokol image/view/sampler pools");
        }
    }

    void initializeTonemap(const sg_swapchain& swapchain) {
        if (tonemap.pipeline.id && outputColor == swapchain.color_format &&
            outputDepth == swapchain.depth_format && outputSamples == swapchain.sample_count) {
            return;
        }
        tonemap.destroy();
        sg_shader_desc desc{};
        desc.vertex_func.source = geometry_shaders::kTonemapVertexSource;
        desc.fragment_func.source = geometry_shaders::kTonemapFragmentSource;
        setUniformBlock(desc.uniform_blocks[1], SG_SHADERSTAGE_FRAGMENT, sizeof(glm::vec4));
        setUniform(desc.uniform_blocks[1], 0, "u_exposure", SG_UNIFORMTYPE_FLOAT4);
        setTexture(desc, 0, 0, 0, "u_hdr_texture");
        desc.label = "ACES explicit sRGB tonemap shader";
        tonemap.shader = sg_make_shader(&desc);
        require(sg_query_shader_state(tonemap.shader) == SG_RESOURCESTATE_VALID,
                "tonemap shader creation failed; check generated tonemap constants and GLSL 4.3 support");
        sg_pipeline_desc pipelineDesc{};
        pipelineDesc.shader = tonemap.shader;
        pipelineDesc.colors[0].pixel_format = swapchain.color_format;
        pipelineDesc.depth.pixel_format = swapchain.depth_format;
        pipelineDesc.depth.compare = SG_COMPAREFUNC_ALWAYS;
        pipelineDesc.depth.write_enabled = false;
        pipelineDesc.sample_count = swapchain.sample_count;
        pipelineDesc.cull_mode = SG_CULLMODE_NONE;
        pipelineDesc.label = "ACES swapchain pipeline";
        tonemap.pipeline = sg_make_pipeline(&pipelineDesc);
        require(sg_query_pipeline_state(tonemap.pipeline) == SG_RESOURCESTATE_VALID,
                "tonemap pipeline creation failed; check swapchain color/depth formats and sample count");
        outputColor = swapchain.color_format;
        outputDepth = swapchain.depth_format;
        outputSamples = swapchain.sample_count;
        if (!hdrSampler.id) {
            sg_sampler_desc samplerDesc{};
            samplerDesc.min_filter = SG_FILTER_LINEAR;
            samplerDesc.mag_filter = SG_FILTER_LINEAR;
            samplerDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
            samplerDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
            samplerDesc.label = "HDR tonemap sampler";
            hdrSampler = sg_make_sampler(&samplerDesc);
            require(sg_query_sampler_state(hdrSampler) == SG_RESOURCESTATE_VALID,
                    "HDR sampler creation failed; check Sokol sampler pool capacity");
        }
    }

    void prepareHdr(RenderContext& render, int width, int height) {
        auto& target = render.hdr;
        if (target.image.id && target.width == width && target.height == height) {
            return;
        }
        target.reset();
        sg_image_desc imageDesc{};
        imageDesc.width = width;
        imageDesc.height = height;
        imageDesc.pixel_format = SG_PIXELFORMAT_RGBA16F;
        imageDesc.sample_count = 1;
        imageDesc.usage.color_attachment = true;
        imageDesc.label = "persistent scene HDR";
        target.image = sg_make_image(&imageDesc);
        require(sg_query_image_state(target.image) == SG_RESOURCESTATE_VALID,
                "HDR image creation failed; check RGBA16F render support, viewport dimensions, and image pool capacity");
        sg_view_desc viewDesc{};
        viewDesc.color_attachment.image = target.image;
        target.attachment = sg_make_view(&viewDesc);
        require(sg_query_view_state(target.attachment) == SG_RESOURCESTATE_VALID,
                "HDR attachment creation failed; check Sokol view pool capacity");
        viewDesc = {};
        viewDesc.texture.image = target.image;
        target.texture = sg_make_view(&viewDesc);
        require(sg_query_view_state(target.texture) == SG_RESOURCESTATE_VALID,
                "HDR texture view creation failed; check Sokol view pool capacity");
        target.width = width;
        target.height = height;
    }
};

GeometryPass::GeometryPass() : impl_(std::make_unique<Impl>()) {}
GeometryPass::~GeometryPass() = default;

bool GeometryPass::drawSurface(RenderContext& context, RID meshRid, size_t surfaceIndex,
                               const std::shared_ptr<Material>& material, const glm::mat4& mvp) {
    const auto* surface = getSurface(context, meshRid, surfaceIndex);
    if (!surface || !material || !finiteMatrix(mvp)) {
        return false;
    }
    if (std::dynamic_pointer_cast<StandardMaterial3D>(material)) {
        std::fprintf(stderr, "GeometryPass: direct PBR draw requires frame and model; submit a render instance instead\n");
        return false;
    }
    auto* gpu = impl_->getShader(material->getShader());
    if (!gpu) {
        return false;
    }
    sg_apply_pipeline(gpu->pipeline);
    sg_bindings bindings{};
    bindings.vertex_buffers[0] = surface->vertexBuffer;
    bindings.index_buffer = surface->indexBuffer;
    sg_apply_bindings(&bindings);
    const VertexUniforms vertex{mvp};
    applyUniforms(0, &vertex, sizeof(vertex));
    FragmentUniforms fragment{};
    if (const auto* parameter = material->getParameter("albedo_color")) {
        if (const auto* color = std::get_if<glm::vec4>(parameter)) {
            fragment.color = *color;
        }
    }
    if (!finiteVector(fragment.color)) {
        return false;
    }
    applyUniforms(1, &fragment, sizeof(fragment));
    sg_draw(0, surface->indexCount, 1);
    return true;
}

bool GeometryPass::drawSurface(RenderContext& context, FrameContext& frame, RID meshRid,
                               size_t surfaceIndex, const std::shared_ptr<Material>& material,
                               const glm::mat4& model) {
    if (!finiteMatrix(model)) {
        return false;
    }
    const auto standard = std::dynamic_pointer_cast<StandardMaterial3D>(material);
    const glm::mat4 mvp = frame.viewProjection * model;
    if (!standard) {
        return drawSurface(context, meshRid, surfaceIndex, material, mvp);
    }
    const auto* surface = getSurface(context, meshRid, surfaceIndex);
    if (!surface || !frame.hasCamera || !frame.clusteredLights || !frame.shadows || !finiteMatrix(mvp)) {
        return false;
    }
    const float determinant = glm::determinant(glm::mat3(model));
    if (!std::isfinite(determinant) || determinant == 0.0f) {
        return false;
    }
    const glm::mat4 normalMatrix = glm::transpose(glm::inverse(model));
    if (!finiteMatrix(normalMatrix)) {
        return false;
    }
    auto* gpu = impl_->getShader(standard->getShader(), true);
    if (!gpu) {
        return false;
    }
    impl_->initializeDefaults();
    const auto& lights = *frame.clusteredLights;
    const auto& shadows = *frame.shadows;
    sg_bindings bindings{};
    bindings.vertex_buffers[0] = surface->vertexBuffer;
    bindings.index_buffer = surface->indexBuffer;
    for (size_t i = 0; i < impl_->defaults.size(); ++i) {
        const auto& assigned = standard->getTexture(static_cast<StandardMaterial3D::TextureSlot>(i));
        const auto& texture = assigned ? assigned : impl_->defaults[i];
        bindings.views[i] = texture->getView();
        bindings.samplers[i] = texture->getSampler();
        if (sg_query_view_state(bindings.views[i]) != SG_RESOURCESTATE_VALID ||
            sg_query_sampler_state(bindings.samplers[i]) != SG_RESOURCESTATE_VALID) {
            return false;
        }
    }
    bindings.views[5] = lights.lights;
    bindings.views[6] = lights.clusters;
    bindings.views[7] = lights.indices;
    bindings.views[8] = shadows.matrices;
    bindings.views[9] = shadows.atlas;
    bindings.samplers[5] = shadows.sampler;
    for (int i = 5; i < 10; ++i) {
        if (sg_query_view_state(bindings.views[i]) != SG_RESOURCESTATE_VALID) {
            return false;
        }
    }
    if (sg_query_sampler_state(shadows.sampler) != SG_RESOURCESTATE_VALID) {
        return false;
    }
    sg_apply_pipeline(gpu->pipeline);
    sg_apply_bindings(&bindings);
    const PbrVertexUniforms vertex{mvp, model, normalMatrix};
    PbrFragmentUniforms fragment{};
    fragment.view = frame.view;
    fragment.baseColor = standard->getBaseColor();
    fragment.material = glm::vec4(standard->getMetallic(), standard->getRoughness(), standard->getNormalScale(), 0.0f);
    fragment.emission = glm::vec4(standard->getEmission(), 0.0f);
    fragment.camera = glm::vec4(frame.cameraPosition, 0.0f);
    fragment.ambient = glm::vec4(context.settings.ambientColor, 0.0f);
    fragment.screen = glm::vec4(context.hdr.width, context.hdr.height, 0.0f, 0.0f);
    fragment.grid = glm::vec4(glm::vec3(lights.grid), static_cast<float>(lights.lightCount));
    fragment.zParameters = lights.zParameters;
    fragment.shadowTexel = glm::vec4(shadows.texelSize, 0.0f, 0.0f);
    if (!finiteMatrix(fragment.view) || !finiteVector(fragment.baseColor) ||
        !finiteVector(fragment.material) || !finiteVector(fragment.emission) ||
        !finiteVector(fragment.camera) || !finiteVector(fragment.ambient) ||
        !finiteVector(fragment.screen) || !finiteVector(fragment.grid) ||
        !finiteVector(fragment.zParameters) || !finiteVector(fragment.shadowTexel)) {
        return false;
    }
    applyUniforms(0, &vertex, sizeof(vertex));
    applyUniforms(1, &fragment, sizeof(fragment));
    sg_draw(0, surface->indexCount, 1);
    return true;
}

void GeometryPass::execute(RenderContext& context, FrameContext& frame) {
    require(frame.depth.has_value() && frame.clusteredLights.has_value() && frame.shadows.has_value(),
            "missing Depth, ClusteredLights, or Shadows; schedule their producer passes before GeometryPass");
    const int width = std::max(1, frame.viewport.width);
    const int height = std::max(1, frame.viewport.height);
    require(context.depth.width == width && context.depth.height == height &&
                frame.depth->attachment.id == context.depth.attachment.id &&
                sg_query_view_state(frame.depth->attachment) == SG_RESOURCESTATE_VALID,
            "depth prepass target is invalid or has different dimensions; produce render.depth for the current viewport");
    require(std::isfinite(context.settings.exposure) && context.settings.exposure >= 0.0f,
            "render.settings.exposure must be finite and nonnegative");
    impl_->prepareHdr(context, width, height);
    impl_->initializeTonemap(frame.swapchain);
    sg_pass pass{};
    pass.attachments.colors[0] = context.hdr.attachment;
    pass.attachments.depth_stencil = frame.depth->attachment;
    pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass.action.colors[0].store_action = SG_STOREACTION_STORE;
    pass.action.colors[0].clear_value = {0.08f, 0.10f, 0.14f, 1.0f};
    pass.action.depth.load_action = SG_LOADACTION_LOAD;
    pass.action.depth.store_action = SG_STOREACTION_STORE;
    pass.label = "clustered forward HDR geometry";
    sg_begin_pass(&pass);
    if (frame.hasCamera) {
        context.instances.forEach([&](RID, const RenderInstance& instance) {
            const auto mesh = instance.mesh.lock();
            if (!mesh || !context.meshes.isValid(mesh->getRID())) {
                return;
            }
            for (size_t i = 0; i < mesh->getSurfaceCount(); ++i) {
                const auto& surface = mesh->getSurface(i);
                if (!surface.material) {
                    continue;
                }
                if (!drawSurface(context, frame, mesh->getRID(), i, surface.material, instance.transform)) {
                    std::fprintf(stderr, "GeometryPass: invalid draw input for mesh RID %u surface %zu\n", mesh->getRID().index, i);
                }
            }
        });
    }
    sg_end_pass();
    sg_pass output{};
    output.swapchain = frame.swapchain;
    output.action.colors[0].load_action = SG_LOADACTION_DONTCARE;
    output.action.colors[0].store_action = SG_STOREACTION_STORE;
    output.action.depth.load_action = SG_LOADACTION_DONTCARE;
    output.action.depth.store_action = SG_STOREACTION_DONTCARE;
    output.action.stencil.load_action = SG_LOADACTION_DONTCARE;
    output.action.stencil.store_action = SG_STOREACTION_DONTCARE;
    output.label = "ACES tonemap to swapchain";
    sg_begin_pass(&output);
    sg_apply_pipeline(impl_->tonemap.pipeline);
    sg_bindings bindings{};
    bindings.views[0] = context.hdr.texture;
    bindings.samplers[0] = impl_->hdrSampler;
    sg_apply_bindings(&bindings);
    const glm::vec4 exposure(context.settings.exposure,
                             frame.swapchain.color_format == SG_PIXELFORMAT_SRGB8A8 ? 1.0f : 0.0f,
                             0.0f, 0.0f);
    applyUniforms(1, &exposure, sizeof(exposure));
    sg_draw(0, 3, 1);
    sg_end_pass();
    frame.sceneColor = SceneColor{context.hdr.texture, frame.swapchain};
}
