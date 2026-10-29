#include "RenderingServer.h"
#include "Mesh.h"
#include "Material.h"
#include "passes/geometry_pass/GeometryPass.h"
#include "sokol_glue.h"

#include <cassert>
#include <limits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace {
std::unique_ptr<RenderingServer, RenderingServerDeleter> instance;
}

RenderingServer::RenderingServer()
    = default;

RenderingServer::~RenderingServer() = default;

void RenderingServerDeleter::operator()(RenderingServer* server) const {
    delete server;
}

void RenderingServer::initialize() {
    assert(!instance);
    assert(sg_isvalid());
    if (!sg_query_features().compute) {
        std::fprintf(stderr, "RenderingServer requires OpenGL 4.3+ with compute shaders\n");
        std::abort();
    }

    instance.reset(new RenderingServer());
}

void RenderingServer::shutdown() {
    instance.reset();
}

RenderingServer& RenderingServer::get() {
    assert(instance);
    return *instance;
}

RID RenderingServer::meshCreate() {
    return render_context_.meshes.create();
}

bool RenderingServer::meshAddSurface(
    RID rid,
    std::span<const Vertex> vertices,
    std::span<const uint32_t> indices
) {
    GPUMesh* mesh = render_context_.meshes.get(rid);

    if (!mesh || vertices.empty() || indices.empty()) {
        return false;
    }

    if (indices.size() % 3 != 0 ||
        indices.size() >
            static_cast<size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    for (uint32_t index : indices) {
        if (index >= vertices.size()) {
            return false;
        }
    }

    mesh->surfaces.reserve(mesh->surfaces.size() + 1);

    auto surface = std::make_unique<GPUSurface>();

    sg_buffer_desc vertexDesc{};
    vertexDesc.usage.vertex_buffer = true;
    vertexDesc.data.ptr = vertices.data();
    vertexDesc.data.size = vertices.size_bytes();
    vertexDesc.label = "Mesh Vertex Buffer";

    surface->vertexBuffer = sg_make_buffer(&vertexDesc);

    if (sg_query_buffer_state(surface->vertexBuffer) !=
        SG_RESOURCESTATE_VALID) {
        return false;
    }

    sg_buffer_desc indexDesc{};
    indexDesc.usage.index_buffer = true;
    indexDesc.data.ptr = indices.data();
    indexDesc.data.size = indices.size_bytes();
    indexDesc.label = "Mesh Index Buffer";

    surface->indexBuffer = sg_make_buffer(&indexDesc);

    if (sg_query_buffer_state(surface->indexBuffer) !=
        SG_RESOURCESTATE_VALID) {
        return false;
    }

    surface->indexCount = static_cast<int>(indices.size());

    mesh->surfaces.push_back(std::move(surface));
    ++render_context_.lighting.sceneVersion;

    return true;
}

void RenderingServer::meshClear(RID rid) {
    GPUMesh* mesh = render_context_.meshes.get(rid);

    if (mesh) {
        mesh->surfaces.clear();
        ++render_context_.lighting.sceneVersion;
    }
}

size_t RenderingServer::meshGetSurfaceCount(RID rid) const {
    const GPUMesh* mesh = render_context_.meshes.get(rid);

    return mesh ? mesh->surfaces.size() : 0;
}

bool RenderingServer::meshDrawSurface(
    RID rid,
    size_t surfaceIndex,
    const std::shared_ptr<Material>& material,
    const glm::mat4& mvp
) {
    auto* geometry = pipeline_.findPass<GeometryPass>();
    return geometry && geometry->drawSurface(
        render_context_, rid, surfaceIndex, material, mvp
    );
}

RID RenderingServer::instanceCreate() {
    return render_context_.instances.create();
}

void RenderingServer::instanceFree(RID rid) {
    if (render_context_.instances.destroy(rid)) {
        ++render_context_.lighting.sceneVersion;
    }
}

void RenderingServer::instanceSetMesh(
    RID rid,
    const std::shared_ptr<Mesh>& mesh
) {
    RenderInstance* instance = render_context_.instances.get(rid);

    if (!instance) {
        return;
    }

    instance->mesh = mesh;
    ++render_context_.lighting.sceneVersion;
}

void RenderingServer::instanceSetTransform(
    RID rid,
    const glm::mat4& transform
) {
    RenderInstance* instance = render_context_.instances.get(rid);

    if (!instance) {
        return;
    }

    if (instance->transform != transform) {
        instance->transform = transform;
        ++render_context_.lighting.sceneVersion;
    }
}

RID RenderingServer::cameraCreate() {
    return render_context_.cameras.create();
}

void RenderingServer::cameraFree(RID camera) {
    if (render_context_.activeCamera == camera) {
        render_context_.activeCamera = {};
    }

    render_context_.cameras.destroy(camera);
}

void RenderingServer::cameraSetView(
    RID camera,
    const glm::mat4& view
) {
    CameraData* data = render_context_.cameras.get(camera);

    if (!data) {
        return;
    }

    data->view = view;
}

void RenderingServer::cameraSetProjection(
    RID camera,
    const glm::mat4& projection
) {
    CameraData* data = render_context_.cameras.get(camera);

    if (!data) {
        return;
    }

    data->projection = projection;
}

void RenderingServer::cameraSetActive(RID camera) {
    if (!render_context_.cameras.isValid(camera)) {
        return;
    }

    render_context_.activeCamera = camera;
}

RID RenderingServer::lightCreate(LightType type) {
    LightData data;
    data.type = type;
    return render_context_.lights.create(data);
}

void RenderingServer::lightFree(RID light) {
    if (render_context_.lights.destroy(light)) {
        ++render_context_.lighting.sceneVersion;
    }
}

void RenderingServer::lightSetData(RID light, const LightData& data) {
    auto* stored = render_context_.lights.get(light);
    if (!stored) return;
    *stored = data;
    ++render_context_.lighting.sceneVersion;
}

bool RenderingServer::setRenderSettings(const RenderSettings& settings) {
    const auto grid = settings.clusterGrid;
    const uint64_t clusters = uint64_t{grid.x} * grid.y * grid.z;
    if (grid.x == 0 || grid.y == 0 || grid.z == 0 ||
        grid.x > 65535 || grid.y > 65535 || grid.z > 65535 ||
        clusters > 262144 || settings.maxLights == 0 || settings.maxLights > 4096 ||
        settings.maxLightsPerCluster == 0 || settings.maxLightsPerCluster > 1024 ||
        clusters * settings.maxLightsPerCluster > 33554432 ||
        settings.maxShadowLights == 0 || settings.maxShadowLights > 64 ||
        settings.cascadeCount == 0 || settings.cascadeCount > 4 ||
        settings.shadowResolution < 16 || settings.shadowResolution > 4096 ||
        !std::isfinite(settings.shadowDistance) || settings.shadowDistance <= 0 ||
        !std::isfinite(settings.shadowBias) || settings.shadowBias < 0 ||
        !std::isfinite(settings.shadowNormalBias) || settings.shadowNormalBias < 0 ||
        !std::isfinite(settings.cascadeSplitLambda) || settings.cascadeSplitLambda < 0 ||
        settings.cascadeSplitLambda > 1 || !std::isfinite(settings.exposure) || settings.exposure <= 0 ||
        !std::isfinite(settings.ambientColor.x) || !std::isfinite(settings.ambientColor.y) ||
        !std::isfinite(settings.ambientColor.z) || glm::any(glm::lessThan(settings.ambientColor, glm::vec3(0)))) {
        return false;
    }
    const uint32_t slots = settings.maxShadowLights * 6;
    const uint32_t columns = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(slots))));
    const uint32_t rows = (slots + columns - 1) / columns;
    const uint32_t maxSize = static_cast<uint32_t>(sg_query_limits().max_image_size_2d);
    if (uint64_t{columns} * settings.shadowResolution > maxSize ||
        uint64_t{rows} * settings.shadowResolution > maxSize) {
        return false;
    }
    render_context_.settings = settings;
    ++render_context_.lighting.sceneVersion;
    return true;
}

const RenderSettings& RenderingServer::getRenderSettings() const {
    return render_context_.settings;
}

FrameContext RenderingServer::prepareFrame() {
    FrameContext frame;
    frame.frameIndex = frame_index_++;
    frame.swapchain = sglue_swapchain();
    frame.viewport = {frame.swapchain.width, frame.swapchain.height};
    if (const auto* camera = render_context_.cameras.get(render_context_.activeCamera)) {
        frame.hasCamera = true;
        frame.view = camera->view;
        frame.projection = camera->projection;
        frame.viewProjection = frame.projection * frame.view;
        frame.inverseView = glm::inverse(frame.view);
        frame.inverseProjection = glm::inverse(frame.projection);
        frame.cameraPosition = glm::vec3(frame.inverseView[3]);
        const glm::vec4 nearPoint = frame.inverseProjection * glm::vec4(0, 0, -1, 1);
        const glm::vec4 farPoint = frame.inverseProjection * glm::vec4(0, 0, 1, 1);
        frame.nearPlane = nearPoint.z / nearPoint.w;
        frame.farPlane = farPoint.z / farPoint.w;
    }
    render_context_.instances.forEach([&](RID, const RenderInstance& instance) {
        const auto mesh = instance.mesh.lock();
        if (!mesh) return;
        auto* gpu = render_context_.meshes.get(mesh->getRID());
        if (!gpu) return;
        for (size_t i = 0; i < gpu->surfaces.size(); ++i) {
            const auto material = i < mesh->getSurfaceCount() ? mesh->getSurface(i).material : nullptr;
            const uint64_t materialVersion = material ? material->getVersion() : 0;
            const uint64_t shaderVersion = material && material->getShader() ? material->getShader()->getVersion() : 0;
            auto& surface = *gpu->surfaces[i];
            if (surface.material.lock() != material || surface.materialVersion != materialVersion ||
                surface.shaderVersion != shaderVersion) {
                surface.material = material;
                surface.materialVersion = materialVersion;
                surface.shaderVersion = shaderVersion;
                ++render_context_.lighting.sceneVersion;
            }
        }
    });
    return frame;
}

void RenderingServer::renderScene() {
    FrameContext frame = prepareFrame();
    if (frame.viewport.width > 0 && frame.viewport.height > 0) {
        pipeline_.execute(render_context_, frame);
    }
    sg_commit();
}

void RenderingServer::free(RID rid) {
    if (render_context_.meshes.destroy(rid)) {
        ++render_context_.lighting.sceneVersion;
    }
}

bool RenderingServer::isValid(RID rid) const {
    return render_context_.meshes.isValid(rid);
}
