#pragma once

#include "modules/math/AABB.h"
#include "modules/resource/Resource.h"

#include "RID.h"
#include "Vertex.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

class Material;

class Mesh : public Resource {
public:
    struct Surface {
        Ref<Material> material;
        AABB bounds;
    };

    static Ref<Mesh> create();

    ~Mesh() override;

    bool addSurface(
        std::span<const Vertex> vertices,
        std::span<const uint32_t> indices
    );

    void clearSurfaces();

    size_t getSurfaceCount() const;

    Surface& getSurface(size_t index);
    const Surface& getSurface(size_t index) const;

    RID getRID() const;

private:
    explicit Mesh(RID rid);

    RID rid_;
    std::vector<Surface> surfaces_;
};
