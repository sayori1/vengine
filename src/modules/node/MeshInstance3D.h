#pragma once

#include "Node3D.h"

#include "modules/resource/Resource.h"
#include "modules/renderer/Mesh.h"
#include "modules/renderer/RID.h"

class MeshInstance3D : public Node3D {
public:
    explicit MeshInstance3D(std::string name = {});
    ~MeshInstance3D() override;

    void setMesh(Ref<Mesh> mesh);
    const Ref<Mesh>& getMesh() const;

protected:
    void invalidateSpatialTransform() override;

private:
    void syncTransform();

    Ref<Mesh> mesh_;
    RID instanceRID_;
};
