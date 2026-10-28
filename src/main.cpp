#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"

namespace {

void init() {
    sg_desc desc{};
    desc.environment = sglue_environment();
    desc.logger.func = slog_func;
    sg_setup(&desc);
}

void frame() {
    sg_pass pass{};
    pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass.action.colors[0].clear_value = {0.08f, 0.10f, 0.14f, 1.0f};
    pass.swapchain = sglue_swapchain();
    sg_begin_pass(&pass);
    sg_end_pass();
    sg_commit();
}

void cleanup() {
    sg_shutdown();
}

}

sapp_desc sokol_main(int, char*[]) {
    sapp_desc desc{};
    desc.init_cb = init;
    desc.frame_cb = frame;
    desc.cleanup_cb = cleanup;
    desc.width = 1280;
    desc.height = 720;
    desc.window_title = "vengine";
    desc.gl_major_version = 3;
    desc.gl_minor_version = 3;
    desc.swap_interval = 1;
    desc.logger.func = slog_func;
    return desc;
}
