#pragma once

#include "modules/node/Node.h"
#include "sokol_app.h"

#include <functional>
#include <memory>
#include <utility>

class Application {
public:
    Application();
    ~Application();

    static sapp_desc createDescriptor();
    static Application& get();

    template<typename T, typename... Args>
    static void changeScene(Args&&... args) {
        sceneFactory_ = [
            ...args = std::forward<Args>(args)
        ]() mutable -> std::unique_ptr<Node> {
            return std::make_unique<T>(std::move(args)...);
        };
    }

    Node* getCurrentScene() const;

    void update(float dt);
    void render();

private:
    static Application* instance_;
    static std::function<std::unique_ptr<Node>()> sceneFactory_;

    std::unique_ptr<Node> currentScene_;

    void applySceneChange();
};
