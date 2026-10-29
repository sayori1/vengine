#pragma once

#include "modules/renderer/RenderPass.h"
#include "modules/renderer/FrameContext.h"
#include "modules/renderer/RID.h"

#include <glm/glm.hpp>

#include <cstddef>
#include <memory>

class Material;
class RenderContext;

class GeometryPass : public RenderPass {
public:
    using Requires = TypeList<>;
    using Produces = TypeList<SceneColor, Depth>;
    using Optional = TypeList<>;

    GeometryPass();
    ~GeometryPass() override;

    void execute(RenderContext& context, FrameContext& frame) override;

    bool drawSurface(
        RenderContext& context,
        RID mesh,
        size_t surfaceIndex,
        const std::shared_ptr<Material>& material,
        const glm::mat4& mvp
    );

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
