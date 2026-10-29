#pragma once

#include "modules/renderer/FrameContext.h"
#include "modules/renderer/RenderPass.h"

#include <memory>

class LightCullingPass final : public RenderPass {
public:
    using Requires = TypeList<Depth>;
    using Produces = TypeList<ClusteredLights>;
    using Optional = TypeList<>;

    LightCullingPass();
    ~LightCullingPass() override;
    void execute(RenderContext& render, FrameContext& frame) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
