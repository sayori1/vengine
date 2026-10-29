#pragma once

#include "RID.h"
#include "RenderContext.h"
#include "FrameContext.h"
#include "RenderPipeline.h"
#include "Vertex.h"

#include <glm/mat4x4.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>

class Material;
class Mesh;

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

    bool meshDrawSurface(
        RID mesh,
        size_t surfaceIndex,
        const std::shared_ptr<Material>& material,
        const glm::mat4& mvp
    );

    RID instanceCreate();
    void instanceFree(RID instance);

    void instanceSetMesh(
        RID instance,
        const std::shared_ptr<Mesh>& mesh
    );

    void instanceSetTransform(
        RID instance,
        const glm::mat4& transform
    );

    RID cameraCreate();
    void cameraFree(RID camera);

    void cameraSetView(
        RID camera,
        const glm::mat4& view
    );

    void cameraSetProjection(
        RID camera,
        const glm::mat4& projection
    );

    void cameraSetActive(RID camera);

    void renderScene();

    void free(RID mesh);
    bool isValid(RID mesh) const;

private:
    RenderingServer();
    ~RenderingServer();

    friend struct RenderingServerDeleter;

    FrameContext prepareFrame();

    RenderContext render_context_;
    RenderPipeline pipeline_;
    uint64_t frame_index_ = 0;
};

struct RenderingServerDeleter {
    void operator()(RenderingServer* server) const;
};
