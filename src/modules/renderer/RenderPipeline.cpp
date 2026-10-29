#include "RenderPipeline.h"
#include "passes/geometry_pass/GeometryPass.h"
#include "passes/depth_pass/DepthPass.h"
#include "passes/light_culling_pass/LightCullingPass.h"
#include "passes/shadow_pass/ShadowPass.h"

RenderPipeline::RenderPipeline() {
    passes_ = render_pipeline_detail::Registration<>{}
                  .addPass<DepthPass>()
                  .addPass<LightCullingPass>()
                  .addPass<ShadowPass>()
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
