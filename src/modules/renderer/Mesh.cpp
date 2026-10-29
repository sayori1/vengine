#include "Mesh.h"

Mesh::Mesh(RID rid)
    : rid_(rid) {}

Mesh::~Mesh() {
    if (rid_.isValid()) {
        RenderingServer::get().free(rid_);
    }
}

Ref<Mesh> Mesh::create(
    std::span<const Vertex> vertices,
    std::span<const uint32_t> indices
) {
    RID rid = RenderingServer::get().createMesh(vertices, indices);

    if (!rid.isValid()) {
        return nullptr;
    }

    return Ref<Mesh>(new Mesh(rid));
}

RID Mesh::getRID() const {
    return rid_;
}
