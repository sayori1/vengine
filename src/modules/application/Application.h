#pragma once

#include "modules/node/Node.h"

class Application {
public:
    Application();
    ~Application();

    Node& getRoot();
    void update(float dt);

private:
    Node root_{"Root"};
};
