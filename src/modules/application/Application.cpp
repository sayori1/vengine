#include "Application.h"

#include "modules/renderer/RenderingServer.h"

#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"

#include <glm/mat4x4.hpp>

#include <cassert>
#include <memory>
#include <utility>

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

    RenderingServer::initialize();

    state.application = std::make_unique<Application>();
}

void frame(void* userData) {
    auto& state = *static_cast<State*>(userData);

    state.application->update(
        static_cast<float>(sapp_frame_duration())
    );

    state.application->render();
}

void cleanup(void* userData) {
    std::unique_ptr<State> state(
        static_cast<State*>(userData)
    );

    state->application.reset();

    RenderingServer::shutdown();

    sg_shutdown();
}

}

Application* Application::instance_ = nullptr;
std::function<std::unique_ptr<Node>()> Application::sceneFactory_;

Application::Application() {
    assert(!instance_);
    instance_ = this;
}

Application::~Application() {
    if (currentScene_) {
        currentScene_->destroy();
        currentScene_.reset();
    }

    instance_ = nullptr;
}

Application& Application::get() {
    assert(instance_);
    return *instance_;
}

void Application::applySceneChange() {
    if (!sceneFactory_) {
        return;
    }

    auto factory = std::move(sceneFactory_);
    sceneFactory_ = {};

    if (currentScene_) {
        currentScene_->destroy();
        currentScene_.reset();
    }

    currentScene_ = factory();
}

Node* Application::getCurrentScene() const {
    return currentScene_.get();
}

void Application::update(float dt) {
    applySceneChange();

    if (currentScene_) {
        currentScene_->update(dt);
    }
}

void Application::render() {
    RenderingServer::get().renderScene();
}

sapp_desc Application::createDescriptor() {
    auto* state = new State;

    sapp_desc desc{};

    desc.user_data = state;
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
