#include "GeometryPass.h"

#include "modules/renderer/Material.h"
#include "modules/renderer/Mesh.h"
#include "modules/renderer/RenderContext.h"
#include "modules/renderer/Shader.h"

#include "sokol_gfx.h"

#include <glm/gtc/type_ptr.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <unordered_map>
#include <variant>

namespace {

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

bool createGPUShader(GPUShader& gpu, const Shader& resource) {
    sg_shader_desc shaderDesc{};
    shaderDesc.vs.source = resource.getVertexSource().c_str();
    shaderDesc.fs.source = resource.getFragmentSource().c_str();
    shaderDesc.vs.uniform_blocks[0].size = sizeof(VertexUniforms);
    shaderDesc.vs.uniform_blocks[0].uniforms[0].name = "u_mvp";
    shaderDesc.vs.uniform_blocks[0].uniforms[0].type = SG_UNIFORMTYPE_MAT4;
    shaderDesc.fs.uniform_blocks[0].size = sizeof(FragmentUniforms);
    shaderDesc.fs.uniform_blocks[0].uniforms[0].name = "u_color";
    shaderDesc.fs.uniform_blocks[0].uniforms[0].type = SG_UNIFORMTYPE_FLOAT4;
    shaderDesc.attrs[0].name = "a_position";
    shaderDesc.attrs[1].name = "a_color";
    shaderDesc.label = "Material Shader";

    gpu.shader = sg_make_shader(&shaderDesc);
    if (sg_query_shader_state(gpu.shader) != SG_RESOURCESTATE_VALID) {
        gpu.destroy();
        return false;
    }

    sg_pipeline_desc pipelineDesc{};
    pipelineDesc.shader = gpu.shader;
    pipelineDesc.layout.buffers[0].stride = sizeof(Vertex);
    pipelineDesc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
    pipelineDesc.layout.attrs[0].offset = offsetof(Vertex, position);
    pipelineDesc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT4;
    pipelineDesc.layout.attrs[1].offset = offsetof(Vertex, color);
    pipelineDesc.index_type = SG_INDEXTYPE_UINT32;
    pipelineDesc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    pipelineDesc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    pipelineDesc.depth.write_enabled = true;
    pipelineDesc.cull_mode = SG_CULLMODE_NONE;
    pipelineDesc.label = "Material Pipeline";

    gpu.pipeline = sg_make_pipeline(&pipelineDesc);
    if (sg_query_pipeline_state(gpu.pipeline) != SG_RESOURCESTATE_VALID) {
        gpu.destroy();
        return false;
    }

    gpu.version = resource.getVersion();
    return true;
}

}

struct GeometryPass::Impl {
    std::unordered_map<const Shader*, std::unique_ptr<GPUShader>> shaders;

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

GeometryPass::GeometryPass()
    : impl_(std::make_unique<Impl>()) {}

GeometryPass::~GeometryPass() = default;

bool GeometryPass::drawSurface(
    RenderContext& context,
    RID meshRid,
    size_t surfaceIndex,
    const std::shared_ptr<Material>& material,
    const glm::mat4& mvp
) {
    const GPUMesh* mesh = context.meshes.get(meshRid);
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
    sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &vertexRange);

    FragmentUniforms fragmentUniforms{};
    if (const auto* parameter = material->getParameter("albedo_color")) {
        if (const auto* color = std::get_if<glm::vec4>(parameter)) {
            fragmentUniforms.color = *color;
        }
    }

    sg_range fragmentRange{};
    fragmentRange.ptr = &fragmentUniforms;
    fragmentRange.size = sizeof(FragmentUniforms);
    sg_apply_uniforms(SG_SHADERSTAGE_FS, 0, &fragmentRange);
    sg_draw(0, surface.indexCount, 1);
    return true;
}

void GeometryPass::execute(RenderContext& context, FrameContext& frame) {
    sg_pass pass{};
    pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass.action.colors[0].store_action = SG_STOREACTION_STORE;
    pass.action.colors[0].clear_value = {0.08f, 0.10f, 0.14f, 1.0f};
    pass.action.depth.load_action = SG_LOADACTION_CLEAR;
    pass.action.depth.store_action = SG_STOREACTION_STORE;
    pass.action.depth.clear_value = 1.0f;
    pass.swapchain = frame.swapchain;

    sg_begin_pass(&pass);

    if (frame.hasCamera) {
        context.instances.forEach([&](RID, RenderInstance& instance) {
            auto mesh = instance.mesh.lock();
            if (!mesh || !context.meshes.isValid(mesh->getRID())) {
                return;
            }

            const glm::mat4 mvp = frame.viewProjection * instance.transform;
            for (size_t i = 0; i < mesh->getSurfaceCount(); ++i) {
                const auto& surface = mesh->getSurface(i);
                if (!surface.material) {
                    continue;
                }

                if (!drawSurface(context, mesh->getRID(), i, surface.material, mvp)) {
                    std::fprintf(stderr, "GeometryPass: failed to draw mesh RID %u surface %zu\n",
                                 mesh->getRID().index, i);
                }
            }
        });
    }

    sg_end_pass();
    frame.sceneColor = SceneColor{frame.swapchain};
    frame.depth = Depth{frame.swapchain};
}
