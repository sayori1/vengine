#include "PointLight3D.h"

#include <utility>

PointLight3D::PointLight3D(std::string name)
    : Light3D(LightType::Point, std::move(name)) {}
