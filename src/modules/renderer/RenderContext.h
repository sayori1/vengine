#pragma once

#include "RIDPool.h"
#include "sokol_gfx.h"

#include <glm/mat4x4.hpp>
#include <memory>
#include <vector>

class Mesh;

struct GPUSurface {
    sg_buffer vertexBuffer{};
    sg_buffer indexBuffer{};
    int indexCount = 0;

    GPUSurface() = default;
    GPUSurface(const GPUSurface&) = delete;
    GPUSurface& operator=(const GPUSurface&) = delete;

    ~GPUSurface() {
        if (vertexBuffer.id != SG_INVALID_ID) {
            sg_destroy_buffer(vertexBuffer);
        }
        if (indexBuffer.id != SG_INVALID_ID) {
            sg_destroy_buffer(indexBuffer);
        }
    }
};

struct GPUMesh {
    std::vector<std::unique_ptr<GPUSurface>> surfaces;
};

struct RenderInstance {
    std::weak_ptr<Mesh> mesh;
    glm::mat4 transform{1.0f};
};

struct CameraData {
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
};

class RenderContext {
public:
    RIDPool<GPUMesh> meshes;
    RIDPool<RenderInstance> instances;
    RIDPool<CameraData> cameras;
    RID activeCamera;
};
