#include "RenderingServer.h"

#include "RIDPool.h"
#include "Material.h"
#include "Mesh.h"
#include "Shader.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"

#include <glm/gtc/type_ptr.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace {

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

struct GPUShader {
    std::weak_ptr<Shader> resource;

    uint64_t version = 0;

    sg_shader shader{};
    sg_pipeline pipeline{};

    GPUShader() = default;

    GPUShader(const GPUShader&) = delete;
    GPUShader& operator=(const GPUShader&) = delete;

    ~GPUShader() {
        destroy();
    }

    void destroy() {
        if (pipeline.id != SG_INVALID_ID) {
            sg_destroy_pipeline(pipeline);
            pipeline = {};
        }

        if (shader.id != SG_INVALID_ID) {
            sg_destroy_shader(shader);
            shader = {};
        }

        version = 0;
    }
};

struct VertexUniforms {
    glm::mat4 mvp{1.0f};
};

struct FragmentUniforms {
    glm::vec4 color{1.0f};
};

static_assert(sizeof(VertexUniforms) == 64);
static_assert(sizeof(FragmentUniforms) == 16);

std::unique_ptr<RenderingServer, RenderingServerDeleter> instance;

bool createGPUShader(GPUShader& gpu, const Shader& resource) {
    sg_shader_desc shaderDesc{};

    shaderDesc.vs.source =
        resource.getVertexSource().c_str();

    shaderDesc.fs.source =
        resource.getFragmentSource().c_str();

    shaderDesc.vs.uniform_blocks[0].size =
        sizeof(VertexUniforms);

    shaderDesc.vs.uniform_blocks[0].uniforms[0].name =
        "u_mvp";

    shaderDesc.vs.uniform_blocks[0].uniforms[0].type =
        SG_UNIFORMTYPE_MAT4;

    shaderDesc.fs.uniform_blocks[0].size =
        sizeof(FragmentUniforms);

    shaderDesc.fs.uniform_blocks[0].uniforms[0].name =
        "u_color";

    shaderDesc.fs.uniform_blocks[0].uniforms[0].type =
        SG_UNIFORMTYPE_FLOAT4;

    shaderDesc.attrs[0].name = "a_position";
    shaderDesc.attrs[1].name = "a_color";

    shaderDesc.label = "Material Shader";

    gpu.shader = sg_make_shader(&shaderDesc);

    if (sg_query_shader_state(gpu.shader) !=
        SG_RESOURCESTATE_VALID) {
        gpu.destroy();
        return false;
    }

    sg_pipeline_desc pipelineDesc{};

    pipelineDesc.shader = gpu.shader;

    pipelineDesc.layout.buffers[0].stride = sizeof(Vertex);

    pipelineDesc.layout.attrs[0].format =
        SG_VERTEXFORMAT_FLOAT3;

    pipelineDesc.layout.attrs[0].offset =
        offsetof(Vertex, position);

    pipelineDesc.layout.attrs[1].format =
        SG_VERTEXFORMAT_FLOAT4;

    pipelineDesc.layout.attrs[1].offset =
        offsetof(Vertex, color);

    pipelineDesc.index_type = SG_INDEXTYPE_UINT32;
    pipelineDesc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;

    pipelineDesc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    pipelineDesc.depth.write_enabled = true;

    pipelineDesc.cull_mode = SG_CULLMODE_NONE;

    pipelineDesc.label = "Material Pipeline";

    gpu.pipeline = sg_make_pipeline(&pipelineDesc);

    if (sg_query_pipeline_state(gpu.pipeline) !=
        SG_RESOURCESTATE_VALID) {
        gpu.destroy();
        return false;
    }

    gpu.version = resource.getVersion();

    return true;
}

}

struct RenderingServer::Impl {
    RIDPool<GPUMesh> meshes;
    RIDPool<RenderInstance> instances;
    RIDPool<CameraData> cameras;

    RID activeCamera;

    std::unordered_map<
        const Shader*,
        std::unique_ptr<GPUShader>
    > shaders;

    GPUShader* getShader(const std::shared_ptr<Shader>& shader) {
        if (!shader) {
            return nullptr;
        }

        for (auto it = shaders.begin(); it != shaders.end();) {
            if (it->second->resource.expired()) {
                it = shaders.erase(it);
            } else {
                ++it;
            }
        }

        auto& gpu = shaders[shader.get()];

        if (!gpu) {
            gpu = std::make_unique<GPUShader>();
        }

        gpu->resource = shader;

        if (gpu->version != shader->getVersion() ||
            gpu->pipeline.id == SG_INVALID_ID) {

            gpu->destroy();

            if (!createGPUShader(*gpu, *shader)) {
                return nullptr;
            }
        }

        return gpu.get();
    }
};

RenderingServer::RenderingServer()
    : impl_(std::make_unique<Impl>()) {}

RenderingServer::~RenderingServer() = default;

void RenderingServerDeleter::operator()(RenderingServer* server) const {
    delete server;
}

void RenderingServer::initialize() {
    assert(!instance);
    assert(sg_isvalid());

    instance.reset(new RenderingServer());
}

void RenderingServer::shutdown() {
    instance.reset();
}

RenderingServer& RenderingServer::get() {
    assert(instance);
    return *instance;
}

RID RenderingServer::meshCreate() {
    return impl_->meshes.create();
}

bool RenderingServer::meshAddSurface(
    RID rid,
    std::span<const Vertex> vertices,
    std::span<const uint32_t> indices
) {
    GPUMesh* mesh = impl_->meshes.get(rid);

    if (!mesh || vertices.empty() || indices.empty()) {
        return false;
    }

    if (indices.size() % 3 != 0 ||
        indices.size() >
            static_cast<size_t>(std::numeric_limits<int>::max())) {
        return false;
    }

    for (uint32_t index : indices) {
        if (index >= vertices.size()) {
            return false;
        }
    }

    mesh->surfaces.reserve(mesh->surfaces.size() + 1);

    auto surface = std::make_unique<GPUSurface>();

    sg_buffer_desc vertexDesc{};
    vertexDesc.type = SG_BUFFERTYPE_VERTEXBUFFER;
    vertexDesc.data.ptr = vertices.data();
    vertexDesc.data.size = vertices.size_bytes();
    vertexDesc.label = "Mesh Vertex Buffer";

    surface->vertexBuffer = sg_make_buffer(&vertexDesc);

    if (sg_query_buffer_state(surface->vertexBuffer) !=
        SG_RESOURCESTATE_VALID) {
        return false;
    }

    sg_buffer_desc indexDesc{};
    indexDesc.type = SG_BUFFERTYPE_INDEXBUFFER;
    indexDesc.data.ptr = indices.data();
    indexDesc.data.size = indices.size_bytes();
    indexDesc.label = "Mesh Index Buffer";

    surface->indexBuffer = sg_make_buffer(&indexDesc);

    if (sg_query_buffer_state(surface->indexBuffer) !=
        SG_RESOURCESTATE_VALID) {
        return false;
    }

    surface->indexCount = static_cast<int>(indices.size());

    mesh->surfaces.push_back(std::move(surface));

    return true;
}

void RenderingServer::meshClear(RID rid) {
    GPUMesh* mesh = impl_->meshes.get(rid);

    if (mesh) {
        mesh->surfaces.clear();
    }
}

size_t RenderingServer::meshGetSurfaceCount(RID rid) const {
    const GPUMesh* mesh = impl_->meshes.get(rid);

    return mesh ? mesh->surfaces.size() : 0;
}

bool RenderingServer::meshDrawSurface(
    RID rid,
    size_t surfaceIndex,
    const std::shared_ptr<Material>& material,
    const glm::mat4& mvp
) {
    const GPUMesh* mesh = impl_->meshes.get(rid);

    if (!mesh || surfaceIndex >= mesh->surfaces.size()) {
        return false;
    }

    if (!material || !material->getShader()) {
        return false;
    }

    GPUShader* gpu = impl_->getShader(material->getShader());

    if (!gpu) {
        return false;
    }

    const GPUSurface& surface = *mesh->surfaces[surfaceIndex];

    sg_apply_pipeline(gpu->pipeline);

    sg_bindings bindings{};
    bindings.vertex_buffers[0] = surface.vertexBuffer;
    bindings.index_buffer = surface.indexBuffer;

    sg_apply_bindings(&bindings);

    VertexUniforms vertexUniforms{};
    vertexUniforms.mvp = mvp;

    sg_range vertexRange{};
    vertexRange.ptr = glm::value_ptr(vertexUniforms.mvp);
    vertexRange.size = sizeof(VertexUniforms);

    sg_apply_uniforms(
        SG_SHADERSTAGE_VS,
        0,
        &vertexRange
    );

    FragmentUniforms fragmentUniforms{};

    if (const auto* parameter =
            material->getParameter("albedo_color")) {

        if (const auto* color =
                std::get_if<glm::vec4>(parameter)) {
            fragmentUniforms.color = *color;
        }
    }

    sg_range fragmentRange{};
    fragmentRange.ptr = &fragmentUniforms;
    fragmentRange.size = sizeof(FragmentUniforms);

    sg_apply_uniforms(
        SG_SHADERSTAGE_FS,
        0,
        &fragmentRange
    );

    sg_draw(0, surface.indexCount, 1);

    return true;
}

RID RenderingServer::instanceCreate() {
    return impl_->instances.create();
}

void RenderingServer::instanceFree(RID rid) {
    impl_->instances.destroy(rid);
}

void RenderingServer::instanceSetMesh(
    RID rid,
    const std::shared_ptr<Mesh>& mesh
) {
    RenderInstance* instance = impl_->instances.get(rid);

    if (!instance) {
        return;
    }

    instance->mesh = mesh;
}

void RenderingServer::instanceSetTransform(
    RID rid,
    const glm::mat4& transform
) {
    RenderInstance* instance = impl_->instances.get(rid);

    if (!instance) {
        return;
    }

    instance->transform = transform;
}

RID RenderingServer::cameraCreate() {
    return impl_->cameras.create();
}

void RenderingServer::cameraFree(RID camera) {
    if (impl_->activeCamera == camera) {
        impl_->activeCamera = {};
    }

    impl_->cameras.destroy(camera);
}

void RenderingServer::cameraSetView(
    RID camera,
    const glm::mat4& view
) {
    CameraData* data = impl_->cameras.get(camera);

    if (!data) {
        return;
    }

    data->view = view;
}

void RenderingServer::cameraSetProjection(
    RID camera,
    const glm::mat4& projection
) {
    CameraData* data = impl_->cameras.get(camera);

    if (!data) {
        return;
    }

    data->projection = projection;
}

void RenderingServer::cameraSetActive(RID camera) {
    if (!impl_->cameras.isValid(camera)) {
        return;
    }

    impl_->activeCamera = camera;
}

void RenderingServer::renderScene() {
    const CameraData* camera =
        impl_->cameras.get(impl_->activeCamera);

    sg_pass pass{};

    pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass.action.colors[0].clear_value = {
        0.08f, 0.10f, 0.14f, 1.0f
    };

    pass.action.depth.load_action = SG_LOADACTION_CLEAR;
    pass.action.depth.clear_value = 1.0f;

    pass.swapchain = sglue_swapchain();

    sg_begin_pass(&pass);

    if (camera) {
        const glm::mat4 viewProjection =
            camera->projection * camera->view;

        impl_->instances.forEach(
            [&](RID, RenderInstance& instance) {
                auto mesh = instance.mesh.lock();

                if (!mesh || !impl_->meshes.isValid(mesh->getRID())) {
                    return;
                }

                const glm::mat4 mvp =
                    viewProjection * instance.transform;

                for (size_t i = 0; i < mesh->getSurfaceCount(); ++i) {
                    const auto& surface = mesh->getSurface(i);

                    if (!surface.material) {
                        continue;
                    }

                    meshDrawSurface(
                        mesh->getRID(),
                        i,
                        surface.material,
                        mvp
                    );
                }
            }
        );
    }

    sg_end_pass();
    sg_commit();
}

void RenderingServer::free(RID rid) {
    impl_->meshes.destroy(rid);
}

bool RenderingServer::isValid(RID rid) const {
    return impl_->meshes.isValid(rid);
}
