#include "DirectionalLight3D.h"

#include <utility>

DirectionalLight3D::DirectionalLight3D(std::string name)
    : Light3D(LightType::Directional, std::move(name)) {}
