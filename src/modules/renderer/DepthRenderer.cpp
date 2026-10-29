#include "DepthRenderer.h"

#include "Material.h"
#include "Mesh.h"
#include "RenderContext.h"
#include "Shader.h"
#include "StandardMaterial3D.h"
#include "Vertex.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>

namespace {
struct DepthPipeline {
    sg_shader shader{};
    sg_pipeline pipeline{};
    std::weak_ptr<Shader> resource;
    uint64_t version = 0;

    DepthPipeline() = default;
    DepthPipeline(const DepthPipeline&) = delete;
    DepthPipeline& operator=(const DepthPipeline&) = delete;

    ~DepthPipeline() {
        reset();
    }

    void reset() {
        if (pipeline.id) {
            sg_destroy_pipeline(pipeline);
            pipeline = {};
        }
        if (shader.id) {
            sg_destroy_shader(shader);
            shader = {};
        }
        version = 0;
    }
};

void requireResource(bool valid, const char* label, const char* resource) {
    if (!valid) {
        std::fprintf(stderr, "DepthRenderer (%s): failed to create %s; inspect Sokol shader and resource diagnostics\n",
                     label, resource);
        std::abort();
    }
}

void setUniform(sg_shader_uniform_block& block, sg_shader_stage stage, uint32_t size,
                const char* name, sg_uniform_type type) {
    block.stage = stage;
    block.size = size;
    block.layout = SG_UNIFORMLAYOUT_STD140;
    block.glsl_uniforms[0].glsl_name = name;
    block.glsl_uniforms[0].type = type;
    block.glsl_uniforms[0].array_count = 1;
}

void createPipeline(DepthPipeline& gpu, const char* vertexSource, const char* fragmentSource,
                    const char* label, bool custom) {
    sg_shader_desc shaderDesc{};
    shaderDesc.vertex_func.source = vertexSource;
    shaderDesc.fragment_func.source = fragmentSource;
    shaderDesc.attrs[0].glsl_name = "a_position";
    setUniform(shaderDesc.uniform_blocks[0], SG_SHADERSTAGE_VERTEX, sizeof(glm::mat4),
               "u_mvp", SG_UNIFORMTYPE_MAT4);
    if (custom) {
        shaderDesc.attrs[1].glsl_name = "a_color";
        setUniform(shaderDesc.uniform_blocks[1], SG_SHADERSTAGE_FRAGMENT, sizeof(glm::vec4),
                   "u_color", SG_UNIFORMTYPE_FLOAT4);
    }
    shaderDesc.label = label;
    gpu.shader = sg_make_shader(&shaderDesc);
    requireResource(sg_query_shader_state(gpu.shader) == SG_RESOURCESTATE_VALID, label, "shader");
    sg_pipeline_desc desc{};
    desc.shader = gpu.shader;
    desc.layout.buffers[0].stride = sizeof(Vertex);
    desc.layout.attrs[0].offset = offsetof(Vertex, position);
    desc.layout.attrs[0].format = SG_VERTEXFORMAT_FLOAT3;
    if (custom) {
        desc.layout.attrs[1].offset = offsetof(Vertex, color);
        desc.layout.attrs[1].format = SG_VERTEXFORMAT_FLOAT4;
    }
    desc.index_type = SG_INDEXTYPE_UINT32;
    desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
    desc.color_count = 0;
    desc.colors[0].pixel_format = SG_PIXELFORMAT_NONE;
    desc.depth.pixel_format = SG_PIXELFORMAT_DEPTH;
    desc.depth.compare = SG_COMPAREFUNC_LESS_EQUAL;
    desc.depth.write_enabled = true;
    desc.sample_count = 1;
    desc.cull_mode = SG_CULLMODE_NONE;
    desc.label = label;
    gpu.pipeline = sg_make_pipeline(&desc);
    requireResource(sg_query_pipeline_state(gpu.pipeline) == SG_RESOURCESTATE_VALID, label, "pipeline");
}

bool finiteMatrix(const glm::mat4& matrix) {
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            if (!std::isfinite(matrix[column][row])) {
                return false;
            }
        }
    }
    return true;
}
}

struct DepthRenderer::Impl {
    std::string vertexSource;
    std::string fragmentSource;
    std::string label;
    DepthPipeline defaultPipeline;
    std::unordered_map<const Shader*, DepthPipeline> customPipelines;

    Impl(const char* vertex, const char* fragment, const char* name)
        : vertexSource(vertex), fragmentSource(fragment), label(name) {}

    DepthPipeline* getPipeline(const std::shared_ptr<Shader>& resource) {
        if (!resource || resource->getVertexSource().empty() || resource->getFragmentSource().empty()) {
            return nullptr;
        }
        auto& gpu = customPipelines[resource.get()];
        if (gpu.resource.lock() != resource || gpu.version != resource->getVersion()) {
            gpu.reset();
            createPipeline(gpu, resource->getVertexSource().c_str(), resource->getFragmentSource().c_str(),
                           label.c_str(), true);
            gpu.resource = resource;
            gpu.version = resource->getVersion();
        }
        return &gpu;
    }
};

DepthRenderer::DepthRenderer(const char* vertexSource, const char* fragmentSource, const char* label)
    : impl_(std::make_unique<Impl>(vertexSource, fragmentSource, label)) {}

DepthRenderer::~DepthRenderer() = default;

void DepthRenderer::draw(RenderContext& render, const glm::mat4& viewProjection) {
    std::erase_if(impl_->customPipelines, [](const auto& entry) {
        return entry.second.resource.expired();
    });
    render.instances.forEach([&](RID, const RenderInstance& instance) {
        const auto mesh = instance.mesh.lock();
        if (!mesh) {
            return;
        }
        const auto* gpu = render.meshes.get(mesh->getRID());
        if (!gpu || !finiteMatrix(instance.transform)) {
            return;
        }
        const glm::mat4 mvp = viewProjection * instance.transform;
        if (!finiteMatrix(mvp)) {
            return;
        }
        const size_t surfaceCount = std::min(gpu->surfaces.size(), mesh->getSurfaceCount());
        for (size_t i = 0; i < surfaceCount; ++i) {
            const auto& surface = gpu->surfaces[i];
            const auto& material = mesh->getSurface(i).material;
            if (!surface || surface->indexCount <= 0 || !material) {
                continue;
            }
            const bool custom = !std::dynamic_pointer_cast<StandardMaterial3D>(material);
            DepthPipeline* pipeline;
            if (custom) {
                pipeline = impl_->getPipeline(material->getShader());
                if (!pipeline) {
                    continue;
                }
            } else {
                pipeline = &impl_->defaultPipeline;
                if (!pipeline->pipeline.id) {
                    createPipeline(*pipeline, impl_->vertexSource.c_str(), impl_->fragmentSource.c_str(),
                                   impl_->label.c_str(), false);
                }
            }
            sg_apply_pipeline(pipeline->pipeline);
            sg_bindings bindings{};
            bindings.vertex_buffers[0] = surface->vertexBuffer;
            bindings.index_buffer = surface->indexBuffer;
            sg_apply_bindings(&bindings);
            const sg_range vertexUniforms{&mvp, sizeof(mvp)};
            sg_apply_uniforms(0, &vertexUniforms);
            if (custom) {
                glm::vec4 color(1.0f);
                if (const auto* parameter = material->getParameter("albedo_color")) {
                    if (const auto* value = std::get_if<glm::vec4>(parameter)) {
                        color = *value;
                    }
                }
                const sg_range fragmentUniforms{&color, sizeof(color)};
                sg_apply_uniforms(1, &fragmentUniforms);
            }
            sg_draw(0, surface->indexCount, 1);
        }
    });
}
