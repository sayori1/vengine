#pragma once

template<typename... Types>
struct TypeList {};

class RenderContext;
struct FrameContext;

class RenderPass {
public:
    virtual ~RenderPass() = default;
    virtual void execute(RenderContext& render, FrameContext& frame) = 0;
};
