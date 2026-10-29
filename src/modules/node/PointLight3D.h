#pragma once

#include "Light3D.h"

class PointLight3D : public Light3D {
public:
    explicit PointLight3D(std::string name = "PointLight3D");
};
