#include "SpotLight3D.h"

#include <utility>

SpotLight3D::SpotLight3D(std::string name)
    : Light3D(LightType::Spot, std::move(name)) {}
