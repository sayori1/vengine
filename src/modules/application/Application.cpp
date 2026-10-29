#include "Application.h"

#include <memory>

#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"

namespace {

struct State {
    std::unique_ptr<Application> application;
};

void init(void* userData) {
    auto& state = *static_cast<State*>(userData);

    sg_desc desc{};
    desc.environment = sglue_environment();
    desc.logger.func = slog_func;
    sg_setup(&desc);

    state.application = std::make_unique<Application>();
}

void frame(void* userData) {
    auto& state = *static_cast<State*>(userData);

    state.application->update(
        static_cast<float>(sapp_frame_duration())
    );

    sg_pass pass{};
    pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass.action.colors[0].clear_value = {0.08f, 0.10f, 0.14f, 1.0f};
    pass.swapchain = sglue_swapchain();

    sg_begin_pass(&pass);
    sg_end_pass();
    sg_commit();
}

void cleanup(void* userData) {
    std::unique_ptr<State> state(static_cast<State*>(userData));

    state->application.reset();
    sg_shutdown();
}

}

Application::Application() = default;

Application::~Application() {
    root_.destroy();
}

Node& Application::getRoot() {
    return root_;
}

void Application::update(float dt) {
    root_.update(dt);
}

sapp_desc Application::createDescriptor() {
    sapp_desc desc{};

    desc.user_data = new State;
    desc.init_userdata_cb = init;
    desc.frame_userdata_cb = frame;
    desc.cleanup_userdata_cb = cleanup;

    desc.width = 1280;
    desc.height = 720;
    desc.window_title = "vengine";

    desc.gl_major_version = 3;
    desc.gl_minor_version = 3;
    desc.swap_interval = 1;

    desc.logger.func = slog_func;

    return desc;
}
