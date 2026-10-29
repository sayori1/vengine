#pragma once

#include "Light3D.h"

class DirectionalLight3D : public Light3D {
public:
    explicit DirectionalLight3D(std::string name = "DirectionalLight3D");
};
