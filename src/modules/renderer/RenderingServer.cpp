#include "RenderingServer.h"
#include "Mesh.h"
#include "passes/geometry_pass/GeometryPass.h"
#include "sokol_glue.h"

#include <cassert>
#include <limits>
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
    vertexDesc.type = SG_BUFFERTYPE_VERTEXBUFFER;
    vertexDesc.data.ptr = vertices.data();
    vertexDesc.data.size = vertices.size_bytes();
    vertexDesc.label = "Mesh Vertex Buffer";

    surface->vertexBuffer = sg_make_buffer(&vertexDesc);

    if (sg_query_buffer_state(surface->vertexBuffer) !=
        SG_RESOURCESTATE_VALID) {
        return false;
    }

    sg_buffer_desc indexDesc{};
    indexDesc.type = SG_BUFFERTYPE_INDEXBUFFER;
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

    return true;
}

void RenderingServer::meshClear(RID rid) {
    GPUMesh* mesh = render_context_.meshes.get(rid);

    if (mesh) {
        mesh->surfaces.clear();
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
    render_context_.instances.destroy(rid);
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
}

void RenderingServer::instanceSetTransform(
    RID rid,
    const glm::mat4& transform
) {
    RenderInstance* instance = render_context_.instances.get(rid);

    if (!instance) {
        return;
    }

    instance->transform = transform;
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
    }
    return frame;
}

void RenderingServer::renderScene() {
    FrameContext frame = prepareFrame();
    pipeline_.execute(render_context_, frame);
    sg_commit();
}

void RenderingServer::free(RID rid) {
    render_context_.meshes.destroy(rid);
}

bool RenderingServer::isValid(RID rid) const {
    return render_context_.meshes.isValid(rid);
}
