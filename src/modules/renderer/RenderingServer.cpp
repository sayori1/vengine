#include "RenderingServer.h"

#include "RIDPool.h"

#include "sokol_gfx.h"

#include <cassert>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace {

struct GPUSurface {
    sg_buffer vertexBuffer{};
    sg_buffer indexBuffer{};
    int indexCount = 0;

    GPUSurface() = default;

    GPUSurface(const GPUSurface&) = delete;
    GPUSurface& operator=(const GPUSurface&) = delete;

    ~GPUSurface() {
        if (vertexBuffer.id != SG_INVALID_ID) {
            sg_destroy_buffer(vertexBuffer);
        }

        if (indexBuffer.id != SG_INVALID_ID) {
            sg_destroy_buffer(indexBuffer);
        }
    }
};

struct GPUMesh {
    std::vector<std::unique_ptr<GPUSurface>> surfaces;
};

std::unique_ptr<RenderingServer, RenderingServerDeleter> instance;

}

struct RenderingServer::Impl {
    RIDPool<GPUMesh> meshes;
};

RenderingServer::RenderingServer()
    : impl_(std::make_unique<Impl>()) {}

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
    return impl_->meshes.create();
}

bool RenderingServer::meshAddSurface(
    RID rid,
    std::span<const Vertex> vertices,
    std::span<const uint32_t> indices
) {
    GPUMesh* mesh = impl_->meshes.get(rid);

    if (!mesh || vertices.empty() || indices.empty()) {
        return false;
    }

    if (indices.size() >
        static_cast<size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    if (indices.size() % 3 != 0) {
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
    vertexDesc.label = "Mesh Surface Vertex Buffer";

    surface->vertexBuffer = sg_make_buffer(&vertexDesc);

    if (sg_query_buffer_state(surface->vertexBuffer) !=
        SG_RESOURCESTATE_VALID) {
        return false;
    }

    sg_buffer_desc indexDesc{};
    indexDesc.type = SG_BUFFERTYPE_INDEXBUFFER;
    indexDesc.data.ptr = indices.data();
    indexDesc.data.size = indices.size_bytes();
    indexDesc.label = "Mesh Surface Index Buffer";

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
    GPUMesh* mesh = impl_->meshes.get(rid);

    if (!mesh) {
        return;
    }

    mesh->surfaces.clear();
}

size_t RenderingServer::meshGetSurfaceCount(RID rid) const {
    const GPUMesh* mesh = impl_->meshes.get(rid);

    if (!mesh) {
        return 0;
    }

    return mesh->surfaces.size();
}

void RenderingServer::meshDrawSurface(
    RID rid,
    size_t surfaceIndex
) const {
    const GPUMesh* mesh = impl_->meshes.get(rid);

    if (!mesh || surfaceIndex >= mesh->surfaces.size()) {
        return;
    }

    const GPUSurface& surface = *mesh->surfaces[surfaceIndex];

    sg_bindings bindings{};
    bindings.vertex_buffers[0] = surface.vertexBuffer;
    bindings.index_buffer = surface.indexBuffer;

    sg_apply_bindings(&bindings);

    sg_draw(0, surface.indexCount, 1);
}

void RenderingServer::free(RID rid) {
    impl_->meshes.destroy(rid);
}

bool RenderingServer::isValid(RID rid) const {
    return impl_->meshes.isValid(rid);
}
