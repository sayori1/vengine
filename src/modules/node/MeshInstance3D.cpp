#include "MeshInstance3D.h"

#include "modules/renderer/RenderingServer.h"

#include <utility>

MeshInstance3D::MeshInstance3D(std::string name)
    : Node3D(std::move(name)) {

    instanceRID_ = RenderingServer::get().instanceCreate();

    syncTransform();
}

MeshInstance3D::~MeshInstance3D() {
    RenderingServer::get().instanceFree(instanceRID_);
}

void MeshInstance3D::setMesh(Ref<Mesh> mesh) {
    if (mesh_ == mesh) {
        return;
    }

    mesh_ = std::move(mesh);

    RenderingServer::get().instanceSetMesh(
        instanceRID_,
        mesh_
    );
}

const Ref<Mesh>& MeshInstance3D::getMesh() const {
    return mesh_;
}

void MeshInstance3D::invalidateSpatialTransform() {
    Node3D::invalidateSpatialTransform();
    syncTransform();
}

void MeshInstance3D::syncTransform() {
    RenderingServer::get().instanceSetTransform(
        instanceRID_,
        getGlobalTransform()
    );
}
