#include "ShadowPass.h"

#include "GeometryShaderSources.h"
#include "modules/renderer/DepthRenderer.h"
#include "modules/renderer/LightData.h"
#include "modules/renderer/RenderContext.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {
[[noreturn]] void fail(const char* message) {
    std::fprintf(stderr, "ShadowPass: %s\n", message);
    std::abort();
}

void requireResource(bool valid, const char* message) {
    if (!valid) {
        fail(message);
    }
}

bool sameLight(const LightData& a, const LightData& b) {
    return a.type == b.type && a.transform == b.transform && a.color == b.color &&
           a.intensity == b.intensity && a.radius == b.radius &&
           a.innerAngle == b.innerAngle && a.outerAngle == b.outerAngle &&
           a.shadowsEnabled == b.shadowsEnabled;
}

bool sameSettings(const RenderSettings& a, const RenderSettings& b) {
    return a.maxShadowLights == b.maxShadowLights && a.shadowResolution == b.shadowResolution &&
           a.cascadeCount == b.cascadeCount && a.shadowDistance == b.shadowDistance &&
           a.shadowBias == b.shadowBias && a.shadowNormalBias == b.shadowNormalBias &&
           a.cascadeSplitLambda == b.cascadeSplitLambda;
}

glm::vec3 lightDirection(const LightData& light) {
    const glm::vec3 direction(light.transform[0]);
    requireResource(glm::dot(direction, direction) > 0.0f, "light has zero world direction");
    return glm::normalize(direction);
}

glm::vec3 upFor(const glm::vec3& direction) {
    return std::abs(direction.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
}


std::array<glm::vec3, 8> cameraCorners(const FrameContext& frame) {
    const glm::mat4 inverse = glm::inverse(frame.viewProjection);
    std::array<glm::vec3, 8> corners{};
    for (int z = 0; z < 2; ++z) {
        for (int y = 0; y < 2; ++y) {
            for (int x = 0; x < 2; ++x) {
                const glm::vec4 point = inverse * glm::vec4(x * 2 - 1, y * 2 - 1, z * 2 - 1, 1);
                requireResource(std::isfinite(point.w) && std::abs(point.w) > 1.0e-8f,
                                "camera inverse view-projection has invalid frustum corners");
                corners[z * 4 + y * 2 + x] = glm::vec3(point) / point.w;
            }
        }
    }
    return corners;
}

glm::mat4 cascadeMatrix(const FrameContext& frame, const std::array<glm::vec3, 8>& corners,
                        const glm::vec3& direction, float nearSplit, float farSplit, int interior) {
    std::array<glm::vec3, 8> slice{};
    glm::vec3 center(0.0f);
    for (int i = 0; i < 4; ++i) {
        const glm::vec3 start = corners[i];
        const glm::vec3 end = corners[i + 4];
        const float startZ = (frame.view * glm::vec4(start, 1)).z;
        const float endZ = (frame.view * glm::vec4(end, 1)).z;
        requireResource(std::abs(endZ - startZ) > 1.0e-8f, "camera frustum has zero view depth");
        slice[i] = glm::mix(start, end, (nearSplit - startZ) / (endZ - startZ));
        slice[i + 4] = glm::mix(start, end, (farSplit - startZ) / (endZ - startZ));
        center += slice[i] + slice[i + 4];
    }
    center /= 8.0f;
    float radius = 0.0f;
    for (const auto& corner : slice) {
        radius = std::max(radius, glm::length(corner - center));
    }
    radius = std::max(0.0625f, std::ceil(radius * 16.0f) / 16.0f);
    radius *= static_cast<float>(interior) / static_cast<float>(interior - 1);
    const glm::mat4 orientation = glm::lookAtLH(glm::vec3(0), direction, upFor(direction));
    const glm::vec3 lightCenter(orientation * glm::vec4(center, 1));
    const float texel = 2.0f * radius / static_cast<float>(interior);
    const float x = std::round(lightCenter.x / texel) * texel;
    const float y = std::round(lightCenter.y / texel) * texel;
    const float padding = std::max(frame.farPlane, radius);
    const glm::mat4 projection = glm::orthoLH_NO(x - radius, x + radius, y - radius, y + radius,
                                               lightCenter.z - radius - padding,
                                               lightCenter.z + radius + padding);
    return projection * orientation;
}
}

struct ShadowPass::Impl {
    DepthRenderer drawer{geometry_shaders::kShadowVertexSource,
                         geometry_shaders::kShadowFragmentSource, "shadow pipeline"};
    sg_buffer staging{};
    uint32_t capacity = 0;
    uint32_t columns = 1;
    uint32_t rows = 1;
    int resolution = 0;
    bool cached = false;
    bool hasDirectional = false;
    bool hadCamera = false;
    uint64_t sceneVersion = 0;
    uint64_t uploadFrame = std::numeric_limits<uint64_t>::max();
    glm::mat4 viewProjection{1.0f};
    glm::mat4 view{1.0f};
    float nearPlane = 0.0f;
    float farPlane = 0.0f;
    RenderSettings settings;
    std::vector<LightData> sources;
    std::vector<glm::uvec4> metadata;

    ~Impl() {
        if (staging.id) {
            sg_destroy_buffer(staging);
        }

    }


    bool allocate(RenderContext& render) {
        auto& lighting = render.lighting;
        const auto& next = render.settings;
        requireResource(next.shadowResolution > 5, "shadowResolution must exceed five pixels");
        requireResource(next.cascadeCount >= 1 && next.cascadeCount <= 4, "cascadeCount must be in [1, 4]");
        requireResource(next.maxShadowLights <= std::numeric_limits<uint32_t>::max() / 6,
                        "maxShadowLights exceeds slot index range");
        const uint32_t slots = std::max(1u, next.maxShadowLights * 6);
        if (lighting.shadowAtlas.id && lighting.shadowBuffer.id && staging.id &&
            capacity == slots && resolution == next.shadowResolution) {
            return false;
        }
        lighting.resetShadows();
        if (staging.id) {
            sg_destroy_buffer(staging);
            staging = {};
        }
        capacity = slots;
        resolution = next.shadowResolution;
        columns = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(slots))));
        rows = (slots + columns - 1) / columns;
        const uint64_t width = uint64_t(columns) * resolution;
        const uint64_t height = uint64_t(rows) * resolution;
        const auto limits = sg_query_limits();
        requireResource(width <= static_cast<uint64_t>(limits.max_image_size_2d) &&
                        height <= static_cast<uint64_t>(limits.max_image_size_2d),
                        "shadow atlas exceeds GPU maximum 2D image size");
        sg_image_desc imageDesc{};
        imageDesc.width = static_cast<int>(width);
        imageDesc.height = static_cast<int>(height);
        imageDesc.pixel_format = SG_PIXELFORMAT_DEPTH;
        imageDesc.sample_count = 1;
        imageDesc.usage.depth_stencil_attachment = true;
        imageDesc.label = "shadow atlas";
        lighting.shadowAtlas = sg_make_image(&imageDesc);
        requireResource(sg_query_image_state(lighting.shadowAtlas) == SG_RESOURCESTATE_VALID,
                        "failed to create shadow atlas");
        sg_view_desc viewDesc{};
        viewDesc.depth_stencil_attachment.image = lighting.shadowAtlas;
        lighting.shadowAttachment = sg_make_view(&viewDesc);
        requireResource(sg_query_view_state(lighting.shadowAttachment) == SG_RESOURCESTATE_VALID,
                        "failed to create shadow attachment");
        viewDesc = {};
        viewDesc.texture.image = lighting.shadowAtlas;
        lighting.shadowTexture = sg_make_view(&viewDesc);
        requireResource(sg_query_view_state(lighting.shadowTexture) == SG_RESOURCESTATE_VALID,
                        "failed to create shadow texture view");
        sg_sampler_desc samplerDesc{};
        samplerDesc.min_filter = SG_FILTER_NEAREST;
        samplerDesc.mag_filter = SG_FILTER_NEAREST;
        samplerDesc.wrap_u = SG_WRAP_CLAMP_TO_EDGE;
        samplerDesc.wrap_v = SG_WRAP_CLAMP_TO_EDGE;
        samplerDesc.compare = SG_COMPAREFUNC_NEVER;
        samplerDesc.label = "shadow manual PCF sampler";
        lighting.shadowSampler = sg_make_sampler(&samplerDesc);
        requireResource(sg_query_sampler_state(lighting.shadowSampler) == SG_RESOURCESTATE_VALID,
                        "failed to create shadow sampler");
        lighting.cpuShadows.assign(capacity, GPUShadow{});
        for (auto& shadow : lighting.cpuShadows) {
            shadow.viewProjection = glm::mat4(0.0f);
        }
        sg_buffer_desc bufferDesc{};
        bufferDesc.size = lighting.cpuShadows.size() * sizeof(GPUShadow);
        bufferDesc.usage.storage_buffer = true;
        bufferDesc.usage.copy_dst = true;
        bufferDesc.label = "persistent shadow matrices";
        lighting.shadowBuffer = sg_make_buffer(&bufferDesc);
        requireResource(sg_query_buffer_state(lighting.shadowBuffer) == SG_RESOURCESTATE_VALID,
                        "failed to create shadow buffer");
        viewDesc = {};
        viewDesc.storage_buffer.buffer = lighting.shadowBuffer;
        lighting.shadowView = sg_make_view(&viewDesc);
        requireResource(sg_query_view_state(lighting.shadowView) == SG_RESOURCESTATE_VALID,
                        "failed to create shadow buffer view");
        bufferDesc.usage = {};
        bufferDesc.usage.staging_buffer = true;
        bufferDesc.usage.write_transient = true;
        bufferDesc.usage.copy_src = true;
        bufferDesc.label = "shadow upload staging";
        staging = sg_make_buffer(&bufferDesc);
        requireResource(sg_query_buffer_state(staging) == SG_RESOURCESTATE_VALID,
                        "failed to create shadow staging buffer");
        cached = false;
        return true;
    }

    bool unchanged(const RenderContext& render, const FrameContext& frame) const {
        if (!cached || sceneVersion != render.lighting.sceneVersion ||
            !sameSettings(settings, render.settings) || sources.size() != render.lighting.sources.size() ||
            metadata.size() != render.lighting.cpuLights.size()) {
            return false;
        }
        for (size_t i = 0; i < sources.size(); ++i) {
            if (!sameLight(sources[i], render.lighting.sources[i]) ||
                metadata[i] != render.lighting.cpuLights[i].metadata) {
                return false;
            }
        }
        return !hasDirectional || (hadCamera == frame.hasCamera && viewProjection == frame.viewProjection &&
               view == frame.view && nearPlane == frame.nearPlane && farPlane == frame.farPlane);
    }

    uint32_t validateAssignments(const RenderContext& render) {
        const auto& lighting = render.lighting;
        requireResource(lighting.sources.size() == lighting.cpuLights.size(),
                        "sources and cpuLights must have matching order and size");
        uint32_t used = 0;
        uint32_t lightCount = 0;
        hasDirectional = false;
        for (size_t i = 0; i < lighting.sources.size(); ++i) {
            const auto& light = lighting.sources[i];
            const auto& gpu = lighting.cpuLights[i];
            requireResource(gpu.metadata.x == static_cast<uint32_t>(light.type),
                            "uploaded light type does not match source");
            if (!light.shadowsEnabled || lightCount >= render.settings.maxShadowLights) {
                requireResource(gpu.metadata.y == std::numeric_limits<uint32_t>::max() &&
                                gpu.metadata.z == 0, "unexpected uploaded shadow assignment");
                continue;
            }
            uint32_t count = 0;
            switch (light.type) {
            case LightType::Directional:
                hasDirectional = true;
                count = render.settings.cascadeCount;
                break;
            case LightType::Point:
                count = 6;
                break;
            case LightType::Spot:
                count = 1;
                break;
            default:
                fail("unknown light type");
            }
            requireResource(count <= capacity - used, "shadow slot capacity exceeded");
            requireResource(gpu.metadata.y == used && gpu.metadata.z == count,
                            "uploaded shadow assignment does not match sequential atlas layout");
            used += count;
            ++lightCount;
        }
        return used;
    }

    void buildShadows(RenderContext& render, const FrameContext& frame) {
        auto& lighting = render.lighting;
        const auto& next = render.settings;
        for (auto& shadow : lighting.cpuShadows) {
            shadow = GPUShadow{};
            shadow.viewProjection = glm::mat4(0.0f);
        }
        std::array<glm::vec3, 8> corners{};
        const float directionalDistance = std::min(frame.farPlane, next.shadowDistance);
        if (hasDirectional && frame.hasCamera && directionalDistance > frame.nearPlane) {
            requireResource(frame.nearPlane > 0.0f && frame.farPlane > frame.nearPlane,
                            "directional shadows require positive, ordered camera depth ranges");
            corners = cameraCorners(frame);
        }
        const std::array<glm::vec3, 6> directions{
            glm::vec3(1, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, 1, 0),
            glm::vec3(0, -1, 0), glm::vec3(0, 0, 1), glm::vec3(0, 0, -1)};
        const std::array<glm::vec3, 6> cubeUps{
            glm::vec3(0, 1, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, -1),
            glm::vec3(0, 0, 1), glm::vec3(0, 1, 0), glm::vec3(0, 1, 0)};
        const float atlasWidth = static_cast<float>(columns) * resolution;
        const float atlasHeight = static_cast<float>(rows) * resolution;
        for (size_t i = 0; i < lighting.sources.size(); ++i) {
            const auto& light = lighting.sources[i];
            const auto& gpu = lighting.cpuLights[i];
            if (light.type == LightType::Directional &&
                (!frame.hasCamera || directionalDistance <= frame.nearPlane)) {
                continue;
            }
            float previousSplit = frame.nearPlane;
            for (uint32_t face = 0; face < gpu.metadata.z; ++face) {
                const uint32_t slot = gpu.metadata.y + face;
                auto& shadow = lighting.cpuShadows[slot];
                shadow.atlasRect = glm::vec4(
                    static_cast<float>((slot % columns) * resolution + 2) / atlasWidth,
                    static_cast<float>((slot / columns) * resolution + 2) / atlasHeight,
                    static_cast<float>(resolution - 4) / atlasWidth,
                    static_cast<float>(resolution - 4) / atlasHeight);
                float far = light.radius;
                if (light.type == LightType::Directional) {
                    const float distance = directionalDistance;
                    const float fraction = static_cast<float>(face + 1) / gpu.metadata.z;
                    const float logarithmic = frame.nearPlane * std::pow(distance / frame.nearPlane, fraction);
                    const float uniform = frame.nearPlane + (distance - frame.nearPlane) * fraction;
                    far = glm::mix(uniform, logarithmic, std::clamp(next.cascadeSplitLambda, 0.0f, 1.0f));
                    shadow.viewProjection = cascadeMatrix(frame, corners, lightDirection(light),
                                                          previousSplit, far, resolution - 4);
                    previousSplit = far;
                } else {
                    requireResource(std::isfinite(light.radius) && light.radius > 0.0f,
                                    "point and spot shadow radius must be finite and positive");
                    const glm::vec3 position(light.transform[3]);
                    const float near = std::min(0.1f, light.radius * 0.01f);
                    glm::vec3 direction;
                    glm::vec3 up;
                    float fov;
                    if (light.type == LightType::Point) {
                        direction = directions[face];
                        up = cubeUps[face];
                        fov = glm::radians(90.0f);
                    } else {
                        requireResource(light.outerAngle > 0.0f && light.outerAngle < 90.0f,
                                        "spot shadow outerAngle must be in (0, 90) degrees");
                        direction = lightDirection(light);
                        up = upFor(direction);
                        fov = glm::radians(light.outerAngle * 2.0f);
                    }
                    shadow.viewProjection = glm::perspectiveLH_NO(fov, 1.0f, near, far) *
                                            glm::lookAtLH(position, position + direction, up);
                }
                shadow.parameters = glm::vec4(far, next.shadowBias, next.shadowNormalBias, 0.0f);
            }
        }
    }

    void update(RenderContext& render, const FrameContext& frame, uint32_t used) {
        requireResource(uploadFrame != frame.frameIndex,
                        "shadow resources changed after upload in the same frame");
        buildShadows(render, frame);
        auto& lighting = render.lighting;
        sg_write_buffer_desc write{};
        write.src.data = {lighting.cpuShadows.data(), lighting.cpuShadows.size() * sizeof(GPUShadow)};
        write.dst.buffer = staging;
        sg_write_buffer_transient(&write);
        sg_copy_buffer_to_buffer_desc copy{};
        copy.src.buffer = staging;
        copy.dst.buffer = lighting.shadowBuffer;
        copy.size = write.src.data.size;
        sg_copy_buffer_to_buffer(&copy);
        uploadFrame = frame.frameIndex;
        sg_pass pass{};
        pass.attachments.depth_stencil = lighting.shadowAttachment;
        pass.action.depth.load_action = SG_LOADACTION_CLEAR;
        pass.action.depth.store_action = SG_STOREACTION_STORE;
        pass.action.depth.clear_value = 1.0f;
        sg_begin_pass(&pass);
        if (used) {

            for (uint32_t slot = 0; slot < used; ++slot) {
                if (lighting.cpuShadows[slot].parameters.x <= 0.0f) {
                    continue;
                }
                const int x = static_cast<int>(slot % columns) * resolution + 2;
                const int y = static_cast<int>(slot / columns) * resolution + 2;
                sg_apply_viewport(x, y, resolution - 4, resolution - 4, false);
                sg_apply_scissor_rect(x, y, resolution - 4, resolution - 4, false);
                drawer.draw(render, lighting.cpuShadows[slot].viewProjection);
            }
        }
        sg_end_pass();
        sceneVersion = lighting.sceneVersion;
        sources = lighting.sources;
        metadata.resize(lighting.cpuLights.size());
        for (size_t i = 0; i < metadata.size(); ++i) {
            metadata[i] = lighting.cpuLights[i].metadata;
        }
        settings = render.settings;
        viewProjection = frame.viewProjection;
        view = frame.view;
        nearPlane = frame.nearPlane;
        farPlane = frame.farPlane;
        hadCamera = frame.hasCamera;
        cached = true;
    }
};

ShadowPass::ShadowPass() : impl_(std::make_unique<Impl>()) {}
ShadowPass::~ShadowPass() = default;

void ShadowPass::execute(RenderContext& render, FrameContext& frame) {

    const bool allocated = impl_->allocate(render);
    const bool changed = allocated || !impl_->unchanged(render, frame);
    const uint32_t used = impl_->validateAssignments(render);
    if (changed) {
        impl_->update(render, frame, used);
    }
    const auto& lighting = render.lighting;
    frame.shadows = Shadows{
        lighting.shadowTexture, lighting.shadowSampler, lighting.shadowView,
        glm::vec2(1.0f / (static_cast<float>(impl_->columns) * impl_->resolution),
                  1.0f / (static_cast<float>(impl_->rows) * impl_->resolution))};
}
