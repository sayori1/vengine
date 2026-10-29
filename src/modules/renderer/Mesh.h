#pragma once

#include "modules/resource/Resource.h"
#include "RenderingServer.h"

#include <cstdint>
#include <span>

class Mesh : public Resource {
public:
    static Ref<Mesh> create(
        std::span<const Vertex> vertices,
        std::span<const uint32_t> indices
    );

    ~Mesh() override;

    RID getRID() const;

private:
    explicit Mesh(RID rid);

    RID rid_;
};
