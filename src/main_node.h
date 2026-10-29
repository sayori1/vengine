#pragma once

#include "modules/node/Node3D.h"

class MainNode : public Node3D {
public:
    MainNode();
    ~MainNode() override = default;
protected:
    void onUpdate(float dt) override;

private:
    Node3D* cube_ = nullptr;
    Node3D* moving_light_ = nullptr;
    float elapsed_ = 0.0f;
};
