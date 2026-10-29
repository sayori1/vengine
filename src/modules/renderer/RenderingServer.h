#pragma once

#include "RID.h"

#include <cstdint>
#include <memory>
#include <span>

struct Vertex {
    float x;
    float y;
    float z;
};

class RenderingServer {
public:
    static void initialize();
    static void shutdown();
    static RenderingServer& get();

    RenderingServer(const RenderingServer&) = delete;
    RenderingServer& operator=(const RenderingServer&) = delete;

    RID createMesh(
        std::span<const Vertex> vertices,
        std::span<const uint32_t> indices
    );

    void free(RID rid);
    bool isValid(RID rid) const;

    void drawMesh(RID rid) const;

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
