#pragma once

#include "modules/renderer/FrameContext.h"
#include "modules/renderer/RenderPass.h"

#include <memory>

class ShadowPass final : public RenderPass {
public:
    using Requires = TypeList<ClusteredLights>;
    using Produces = TypeList<Shadows>;
    using Optional = TypeList<Depth>;

    ShadowPass();
    ~ShadowPass() override;
    void execute(RenderContext& render, FrameContext& frame) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
