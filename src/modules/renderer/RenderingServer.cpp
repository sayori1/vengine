#include "RenderingServer.h"

#include "RIDPool.h"
#include "sokol_gfx.h"

#include <cassert>
#include <limits>
#include <memory>

namespace {

std::unique_ptr<RenderingServer, RenderingServerDeleter> instance;

struct GPUMesh {
    sg_buffer vertexBuffer{};
    sg_buffer indexBuffer{};
    int indexCount = 0;

    GPUMesh(
        sg_buffer vertexBuffer,
        sg_buffer indexBuffer,
        int indexCount
    )
        : vertexBuffer(vertexBuffer),
          indexBuffer(indexBuffer),
          indexCount(indexCount) {}

    ~GPUMesh() {
        if (vertexBuffer.id != SG_INVALID_ID) {
            sg_destroy_buffer(vertexBuffer);
        }

        if (indexBuffer.id != SG_INVALID_ID) {
            sg_destroy_buffer(indexBuffer);
        }
    }

    GPUMesh(const GPUMesh&) = delete;
    GPUMesh& operator=(const GPUMesh&) = delete;
};

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

RID RenderingServer::createMesh(
    std::span<const Vertex> vertices,
    std::span<const uint32_t> indices
) {
    if (vertices.empty() || indices.empty()) {
        return {};
    }

    if (indices.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return {};
    }

    sg_buffer_desc vertexDesc{};
    vertexDesc.data.ptr = vertices.data();
    vertexDesc.data.size = vertices.size_bytes();
    vertexDesc.label = "Mesh Vertex Buffer";

    sg_buffer vertexBuffer = sg_make_buffer(&vertexDesc);

    if (sg_query_buffer_state(vertexBuffer) != SG_RESOURCESTATE_VALID) {
        sg_destroy_buffer(vertexBuffer);
        return {};
    }

    sg_buffer_desc indexDesc{};
    indexDesc.type = SG_BUFFERTYPE_INDEXBUFFER;
    indexDesc.data.ptr = indices.data();
    indexDesc.data.size = indices.size_bytes();
    indexDesc.label = "Mesh Index Buffer";

    sg_buffer indexBuffer = sg_make_buffer(&indexDesc);

    if (sg_query_buffer_state(indexBuffer) != SG_RESOURCESTATE_VALID) {
        sg_destroy_buffer(vertexBuffer);
        sg_destroy_buffer(indexBuffer);
        return {};
    }

    return impl_->meshes.create(
        vertexBuffer,
        indexBuffer,
        static_cast<int>(indices.size())
    );
}

void RenderingServer::free(RID rid) {
    impl_->meshes.destroy(rid);
}

bool RenderingServer::isValid(RID rid) const {
    return impl_->meshes.isValid(rid);
}

void RenderingServer::drawMesh(RID rid) const {
    const GPUMesh* mesh = impl_->meshes.get(rid);

    if (!mesh) {
        return;
    }

    sg_bindings bindings{};
    bindings.vertex_buffers[0] = mesh->vertexBuffer;
    bindings.index_buffer = mesh->indexBuffer;

    sg_apply_bindings(&bindings);
    sg_draw(0, mesh->indexCount, 1);
}
