#include "RenderPipeline.h"
#include "passes/geometry_pass/GeometryPass.h"

RenderPipeline::RenderPipeline() {
    passes_ = render_pipeline_detail::Registration<>{}
                  .addPass<GeometryPass>()
                  .finish();
}

void RenderPipeline::execute(RenderContext& render, FrameContext& frame) {
    for (auto& entry : passes_) {
        if (entry.enabled) {
            entry.pass->execute(render, frame);
        }
    }
}
