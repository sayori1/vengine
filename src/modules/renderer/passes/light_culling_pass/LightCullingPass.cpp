#include "LightCullingPass.h"

#include "GeometryShaderSources.h"
#include "modules/renderer/RenderContext.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
[[noreturn]] void fail(const char* message) {
    std::fprintf(stderr, "LightCullingPass: %s\n", message);
    std::abort();
}

void require(bool valid, const char* message) {
    if (!valid) {
        fail(message);
    }
}

struct alignas(16) CullingUniforms {
    glm::mat4 view;
    glm::mat4 inverseProjection;
    glm::vec4 viewport;
    glm::vec4 grid;
    glm::vec4 counts;
};

static_assert(sizeof(CullingUniforms) == 176);
static_assert(offsetof(CullingUniforms, inverseProjection) == 64);
static_assert(offsetof(CullingUniforms, viewport) == 128);
static_assert(offsetof(CullingUniforms, grid) == 144);
static_assert(offsetof(CullingUniforms, counts) == 160);

size_t bufferBytes(uint64_t count, size_t stride) {
    require(count <= std::numeric_limits<size_t>::max() / stride,
            "storage buffer size exceeds host address space; reduce cluster grid or light limits");
    const size_t bytes = static_cast<size_t>(std::max(uint64_t{1}, count)) * stride;
    require(bytes <= static_cast<size_t>(std::numeric_limits<int>::max()),
            "storage buffer exceeds Sokol signed-int size limit; reduce cluster grid or light limits");
    return bytes;
}

void createStorage(sg_buffer& buffer, sg_view& view, size_t size, bool transient, const char* label) {
    sg_buffer_desc desc{};
    desc.size = size;
    desc.usage.storage_buffer = true;
    desc.usage.write_transient = transient;
    desc.label = label;
    buffer = sg_make_buffer(&desc);
    if (sg_query_buffer_state(buffer) != SG_RESOURCESTATE_VALID) {
        std::fprintf(stderr, "LightCullingPass: failed to allocate %s (%zu bytes); reduce light limits or cluster grid and inspect Sokol diagnostics\n", label, size);
        std::abort();
    }
    sg_view_desc viewDesc{};
    viewDesc.storage_buffer.buffer = buffer;
    viewDesc.label = label;
    view = sg_make_view(&viewDesc);
    if (sg_query_view_state(view) != SG_RESOURCESTATE_VALID) {
        std::fprintf(stderr, "LightCullingPass: failed to create %s storage view; inspect Sokol diagnostics and view pool capacity\n", label);
        std::abort();
    }
}
}

struct LightCullingPass::Impl {
    sg_shader shader{};
    sg_pipeline pipeline{};
    glm::uvec3 grid_{0};
    uint32_t max_lights_ = 0;
    uint32_t max_per_cluster_ = 0;
    uint32_t max_shadow_lights_ = 0;
    uint32_t cascade_count_ = 0;
    bool overflow_reported_ = false;

    ~Impl() {
        if (pipeline.id) {
            sg_destroy_pipeline(pipeline);
        }
        if (shader.id) {
            sg_destroy_shader(shader);
        }
    }

    void initialize() {
        if (pipeline.id) {
            return;
        }
        require(sg_query_features().compute,
                "compute shaders are unavailable; select a compute-capable Sokol backend/device (OpenGL requires 4.3+)");
        require(sg_query_limits().max_storage_buffer_bindings_per_stage >= 3,
                "compute requires three storage buffer bindings; select a device supporting at least three");
        sg_shader_desc desc{};
        desc.compute_func.source = geometry_shaders::kLightCullingComputeSource;
        auto& block = desc.uniform_blocks[0];
        block.stage = SG_SHADERSTAGE_COMPUTE;
        block.size = sizeof(CullingUniforms);
        block.layout = SG_UNIFORMLAYOUT_STD140;
        const char* names[] = {"u_view", "u_inverse_projection", "u_viewport", "u_grid", "u_counts"};
        for (int i = 0; i < 5; ++i) {
            block.glsl_uniforms[i].glsl_name = names[i];
            block.glsl_uniforms[i].type = i < 2 ? SG_UNIFORMTYPE_MAT4 : SG_UNIFORMTYPE_FLOAT4;
            block.glsl_uniforms[i].array_count = 1;
        }
        for (int i = 0; i < 3; ++i) {
            auto& storage = desc.views[i].storage_buffer;
            storage.stage = SG_SHADERSTAGE_COMPUTE;
            storage.readonly = i == 0;
            storage.glsl_binding_n = static_cast<uint8_t>(i);
        }
        desc.mtl_threads_per_threadgroup = {1, 1, 1};
        desc.label = "light culling compute shader";
        shader = sg_make_shader(&desc);
        require(sg_query_shader_state(shader) == SG_RESOURCESTATE_VALID,
                "failed to create compute shader; inspect Sokol compiler diagnostics and generated kLightCullingComputeSource");
        sg_pipeline_desc pipelineDesc{};
        pipelineDesc.compute = true;
        pipelineDesc.shader = shader;
        pipelineDesc.label = "light culling compute pipeline";
        pipeline = sg_make_pipeline(&pipelineDesc);
        require(sg_query_pipeline_state(pipeline) == SG_RESOURCESTATE_VALID,
                "failed to create compute pipeline; inspect Sokol diagnostics and pipeline pool capacity");
    }

    void prepareResources(RenderContext& render) {
        const auto& settings = render.settings;
        const auto grid = settings.clusterGrid;
        constexpr uint32_t kExactFloatInteger = 1u << 24;
        require(grid.x > 0 && grid.y > 0 && grid.z > 0,
                "clusterGrid dimensions must be positive");
        require(grid.x <= 65535 && grid.y <= 65535 && grid.z <= 65535,
                "clusterGrid exceeds portable dispatch limits (65535 per axis); reduce grid dimensions");
        require(settings.maxLights <= kExactFloatInteger && settings.maxLightsPerCluster <= kExactFloatInteger,
                "light limits exceed exact float uniform representation (16777216); reduce limits");
        const uint64_t clusterCount = uint64_t{grid.x} * grid.y * grid.z;
        require(clusterCount <= std::numeric_limits<uint32_t>::max(),
                "cluster count exceeds 32-bit shader addressing; reduce clusterGrid");
        const uint64_t indexCount = clusterCount * settings.maxLightsPerCluster;
        require(indexCount <= std::numeric_limits<uint32_t>::max(),
                "cluster index count exceeds 32-bit shader addressing; reduce grid or maxLightsPerCluster");
        if (max_shadow_lights_ != settings.maxShadowLights || cascade_count_ != settings.cascadeCount) {
            overflow_reported_ = false;
            max_shadow_lights_ = settings.maxShadowLights;
            cascade_count_ = settings.cascadeCount;
        }
        auto& lighting = render.lighting;
        if (lighting.lightBuffer.id && lighting.lightView.id && lighting.clusterBuffer.id &&
            lighting.clusterView.id && lighting.indexBuffer.id && lighting.indexView.id &&
            grid_ == grid && max_lights_ == settings.maxLights &&
            max_per_cluster_ == settings.maxLightsPerCluster) {
            return;
        }
        lighting.resetClusters();
        createStorage(lighting.lightBuffer, lighting.lightView,
                      bufferBytes(settings.maxLights, sizeof(GPULight)), true, "cluster lights");
        createStorage(lighting.clusterBuffer, lighting.clusterView,
                      bufferBytes(clusterCount, sizeof(glm::uvec4)), false, "cluster offsets counts overflow");
        createStorage(lighting.indexBuffer, lighting.indexView,
                      bufferBytes(indexCount, sizeof(uint32_t)), false, "cluster light indices");
        grid_ = grid;
        max_lights_ = settings.maxLights;
        max_per_cluster_ = settings.maxLightsPerCluster;
        overflow_reported_ = false;
    }

    void collectLights(RenderContext& render) {
        auto& lighting = render.lighting;
        const auto& settings = render.settings;
        lighting.sources.clear();
        lighting.cpuLights.clear();
        lighting.sources.reserve(std::min<size_t>(settings.maxLights, render.lights.size()));
        lighting.cpuLights.reserve(std::min<size_t>(settings.maxLights, render.lights.size()));
        uint32_t shadowLights = 0;
        uint32_t shadowBase = 0;
        render.lights.forEach([&](RID, const LightData& source) {
            if (lighting.sources.size() >= settings.maxLights) {
                if (!overflow_reported_) {
                    std::fprintf(stderr, "LightCullingPass: %zu scene lights exceed maxLights=%u; excess lights omitted until the limit is increased\n",
                                 render.lights.size(), settings.maxLights);
                    overflow_reported_ = true;
                }
                return;
            }
            lighting.sources.push_back(source);
            const glm::vec3 worldDirection(source.transform * glm::vec4(1, 0, 0, 0));
            const float length = glm::length(worldDirection);
            require(std::isfinite(length) && length > 0.0f,
                    "light world transform has an invalid +X direction; provide a finite nonzero transform axis");
            GPULight light{};
            light.positionRadius = glm::vec4(glm::vec3(source.transform[3]), source.radius);
            light.directionOuter = glm::vec4(worldDirection / length, std::cos(glm::radians(source.outerAngle)));
            light.colorIntensity = glm::vec4(source.color, source.intensity);
            light.parameters = glm::vec4(std::cos(glm::radians(source.innerAngle)), 0, 0, 0);
            const uint32_t type = static_cast<uint32_t>(source.type);
            require(type <= 2, "light has an unsupported type; use Directional, Point, or Spot");
            light.metadata = glm::uvec4(type, std::numeric_limits<uint32_t>::max(), 0, 0);
            if (source.shadowsEnabled && shadowLights < settings.maxShadowLights) {
                const uint32_t count = source.type == LightType::Directional
                    ? std::clamp(settings.cascadeCount, 1u, 4u)
                    : source.type == LightType::Point ? 6u : 1u;
                light.metadata.y = shadowBase;
                light.metadata.z = count;
                shadowBase += count;
                ++shadowLights;
            }
            lighting.cpuLights.push_back(light);
        });
    }
};

LightCullingPass::LightCullingPass() : impl_(std::make_unique<Impl>()) {}
LightCullingPass::~LightCullingPass() = default;

void LightCullingPass::execute(RenderContext& render, FrameContext& frame) {
    impl_->initialize();
    require(std::isfinite(frame.nearPlane) && std::isfinite(frame.farPlane) &&
                frame.nearPlane > 0.0f && frame.farPlane > frame.nearPlane,
            "logarithmic clusters require finite camera planes with 0 < near < far");
    const float depthRatio = frame.farPlane / frame.nearPlane;
    const float depthLog = std::log(depthRatio);
    const float zScale = static_cast<float>(render.settings.clusterGrid.z) / depthLog;
    require(std::isfinite(depthRatio) && depthRatio > 1.0f && std::isfinite(zScale) && zScale > 0.0f,
            "camera depth range cannot be represented by logarithmic cluster uniforms; reduce far/near ratio or increase plane separation");
    impl_->prepareResources(render);
    impl_->collectLights(render);
    auto& lighting = render.lighting;
    GPULight emptyLight{};
    sg_write_buffer_desc write{};
    write.dst.buffer = lighting.lightBuffer;
    write.src.data.ptr = lighting.cpuLights.empty() ? &emptyLight : lighting.cpuLights.data();
    write.src.data.size = lighting.cpuLights.empty() ? sizeof(emptyLight) : lighting.cpuLights.size() * sizeof(GPULight);
    sg_write_buffer_transient(&write);
    const auto grid = render.settings.clusterGrid;
    const auto lightCount = static_cast<uint32_t>(lighting.cpuLights.size());
    const CullingUniforms uniforms{
        frame.view,
        frame.inverseProjection,
        glm::vec4(std::max(1, frame.viewport.width), std::max(1, frame.viewport.height), frame.nearPlane, frame.farPlane),
        glm::vec4(glm::vec3(grid), static_cast<float>(render.settings.maxLightsPerCluster)),
        glm::vec4(static_cast<float>(lightCount), 0, 0, 0)};
    sg_pass pass{};
    pass.compute = true;
    pass.label = "light culling";
    sg_begin_pass(&pass);
    sg_apply_pipeline(impl_->pipeline);
    sg_bindings bindings{};
    bindings.views[0] = lighting.lightView;
    bindings.views[1] = lighting.clusterView;
    bindings.views[2] = lighting.indexView;
    sg_apply_bindings(&bindings);
    const sg_range range{&uniforms, sizeof(uniforms)};
    sg_apply_uniforms(0, &range);
    sg_dispatch(static_cast<int>(grid.x), static_cast<int>(grid.y), static_cast<int>(grid.z));
    sg_end_pass();
    frame.clusteredLights = ClusteredLights{
        lighting.lightView, lighting.clusterView, lighting.indexView, glm::uvec4(grid, 0),
        glm::vec4(frame.nearPlane, frame.farPlane, zScale, 0),
        lightCount, render.settings.maxLightsPerCluster};
}
