#pragma once

#include "modules/node/Node.h"
#include "sokol_app.h"

class Application {
public:
    Application();
    ~Application();

    static sapp_desc createDescriptor();

    Node& getRoot();
    void update(float dt);

private:
    Node root_{"Root"};
};
