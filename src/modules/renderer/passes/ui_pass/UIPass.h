#pragma once

#include "modules/renderer/RenderPass.h"

#include <memory>

class UIPass : public RenderPass {
public:
    using Requires = TypeList<>;
    using Produces = TypeList<>;
    using Optional = TypeList<>;

    UIPass();
    ~UIPass() override;
    void execute(RenderContext& render, FrameContext& frame) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
