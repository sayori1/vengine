#pragma once

#include "RID.h"
#include "Vertex.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

class RenderingServer {
public:
    static void initialize();
    static void shutdown();
    static RenderingServer& get();

    RenderingServer(const RenderingServer&) = delete;
    RenderingServer& operator=(const RenderingServer&) = delete;

    RID meshCreate();

    bool meshAddSurface(
        RID mesh,
        std::span<const Vertex> vertices,
        std::span<const uint32_t> indices
    );

    void meshClear(RID mesh);

    size_t meshGetSurfaceCount(RID mesh) const;

    void meshDrawSurface(RID mesh, size_t surfaceIndex) const;

    void free(RID rid);
    bool isValid(RID rid) const;

private:
    RenderingServer();
    ~RenderingServer();

    friend struct RenderingServerDeleter;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct RenderingServerDeleter {
    void operator()(RenderingServer* server) const;
};
