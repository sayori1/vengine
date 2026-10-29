#pragma once

#include "modules/renderer/FrameContext.h"
#include "modules/renderer/RenderPass.h"

#include <memory>

class DepthPass final : public RenderPass {
public:
    using Requires = TypeList<>;
    using Produces = TypeList<Depth>;
    using Optional = TypeList<>;

    DepthPass();
    ~DepthPass() override;
    void execute(RenderContext& render, FrameContext& frame) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
