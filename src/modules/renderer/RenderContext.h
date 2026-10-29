#pragma once

#include "RIDPool.h"
#include "LightData.h"
#include "RenderSettings.h"
#include "sokol_gfx.h"

#include <glm/mat4x4.hpp>
#include <memory>
#include <vector>

class Mesh;
class Material;

struct GPUSurface {
    sg_buffer vertexBuffer{};
    sg_buffer indexBuffer{};
    int indexCount = 0;
    std::weak_ptr<Material> material;
    uint64_t materialVersion = 0;
    uint64_t shaderVersion = 0;

    GPUSurface() = default;
    GPUSurface(const GPUSurface&) = delete;
    GPUSurface& operator=(const GPUSurface&) = delete;
    ~GPUSurface();
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

struct DepthTarget {
    sg_image image{};
    sg_view attachment{};
    sg_view texture{};
    int width = 0;
    int height = 0;
    DepthTarget() = default;
    DepthTarget(const DepthTarget&) = delete;
    DepthTarget& operator=(const DepthTarget&) = delete;
    ~DepthTarget();
    void reset();
};

struct ColorTarget {
    sg_image image{};
    sg_view attachment{};
    sg_view texture{};
    int width = 0;
    int height = 0;
    ColorTarget() = default;
    ColorTarget(const ColorTarget&) = delete;
    ColorTarget& operator=(const ColorTarget&) = delete;
    ~ColorTarget();
    void reset();
};

struct LightingResources {
    sg_buffer lightBuffer{};
    sg_view lightView{};
    sg_buffer clusterBuffer{};
    sg_view clusterView{};
    sg_buffer indexBuffer{};
    sg_view indexView{};
    sg_buffer shadowBuffer{};
    sg_view shadowView{};
    sg_image shadowAtlas{};
    sg_view shadowAttachment{};
    sg_view shadowTexture{};
    sg_sampler shadowSampler{};
    std::vector<LightData> sources;
    std::vector<GPULight> cpuLights;
    std::vector<GPUShadow> cpuShadows;
    uint64_t sceneVersion = 1;
    LightingResources() = default;
    LightingResources(const LightingResources&) = delete;
    LightingResources& operator=(const LightingResources&) = delete;
    ~LightingResources();
    void resetClusters();
    void resetShadows();
};

class RenderContext {
public:
    RIDPool<GPUMesh> meshes;
    RIDPool<RenderInstance> instances;
    RIDPool<CameraData> cameras;
    RIDPool<LightData> lights;
    RID activeCamera;
    RenderSettings settings;
    DepthTarget depth;
    ColorTarget hdr;
    LightingResources lighting;
};
