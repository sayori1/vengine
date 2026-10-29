#pragma once

#include "Light3D.h"

class SpotLight3D : public Light3D {
public:
    explicit SpotLight3D(std::string name = "SpotLight3D");
};
