#include "Mesh.h"

#include <algorithm>
#include <limits>

Mesh::Mesh(RID rid)
    : rid_(rid) {}

Mesh::~Mesh() {
    if (rid_.isValid()) {
        RenderingServer::get().free(rid_);
    }
}

Ref<Mesh> Mesh::create() {
    RID rid = RenderingServer::get().meshCreate();

    if (!rid.isValid()) {
        return nullptr;
    }

    return Ref<Mesh>(new Mesh(rid));
}

bool Mesh::addSurface(
    std::span<const Vertex> vertices,
    std::span<const uint32_t> indices
) {
    if (vertices.empty()) {
        return false;
    }

    AABB bounds;

    bounds.min = glm::vec3(std::numeric_limits<float>::max());
    bounds.max = glm::vec3(std::numeric_limits<float>::lowest());

    for (const Vertex& vertex : vertices) {
        bounds.min = glm::min(bounds.min, vertex.position);
        bounds.max = glm::max(bounds.max, vertex.position);
    }

    if (!RenderingServer::get().meshAddSurface(rid_, vertices, indices)) {
        return false;
    }

    surfaces_.push_back({nullptr, bounds});

    return true;
}

void Mesh::clearSurfaces() {
    RenderingServer::get().meshClear(rid_);
    surfaces_.clear();
}

size_t Mesh::getSurfaceCount() const {
    return surfaces_.size();
}

Mesh::Surface& Mesh::getSurface(size_t index) {
    return surfaces_.at(index);
}

const Mesh::Surface& Mesh::getSurface(size_t index) const {
    return surfaces_.at(index);
}

RID Mesh::getRID() const {
    return rid_;
}
