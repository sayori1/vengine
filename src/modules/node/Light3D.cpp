#include "Light3D.h"

#include "modules/renderer/RenderingServer.h"

#include <cmath>
#include <utility>

Light3D::Light3D(LightType type, std::string name)
    : Node3D(std::move(name)), light_rid_(RenderingServer::get().lightCreate(type)) {
    light_data_.type = type;
    syncData();
}

Light3D::~Light3D() {
    RenderingServer::get().lightFree(light_rid_);
}

void Light3D::setColor(const glm::vec3& color) {
    if (!std::isfinite(color.x) || !std::isfinite(color.y) || !std::isfinite(color.z) ||
        glm::any(glm::lessThan(color, glm::vec3(0.0f)))) {
        return;
    }
    light_data_.color = color;
    syncData();
}

const glm::vec3& Light3D::getColor() const { return light_data_.color; }

void Light3D::setIntensity(float intensity) {
    if (!std::isfinite(intensity) || intensity < 0.0f) {
        return;
    }
    light_data_.intensity = intensity;
    syncData();
}

float Light3D::getIntensity() const { return light_data_.intensity; }

void Light3D::setRadius(float radius) {
    if (!std::isfinite(radius) || radius <= 0.0f) {
        return;
    }
    light_data_.radius = radius;
    syncData();
}

float Light3D::getRadius() const { return light_data_.radius; }

void Light3D::setInnerAngle(float degrees) {
    if (!std::isfinite(degrees) || degrees < 0.0f || degrees >= light_data_.outerAngle) {
        return;
    }
    light_data_.innerAngle = degrees;
    syncData();
}

float Light3D::getInnerAngle() const { return light_data_.innerAngle; }

void Light3D::setOuterAngle(float degrees) {
    if (!std::isfinite(degrees) || degrees <= light_data_.innerAngle || degrees >= 90.0f) {
        return;
    }
    light_data_.outerAngle = degrees;
    syncData();
}

float Light3D::getOuterAngle() const { return light_data_.outerAngle; }

void Light3D::setShadowsEnabled(bool enabled) {
    light_data_.shadowsEnabled = enabled;
    syncData();
}

bool Light3D::getShadowsEnabled() const { return light_data_.shadowsEnabled; }
LightType Light3D::getLightType() const { return light_data_.type; }
RID Light3D::getRID() const { return light_rid_; }

void Light3D::invalidateSpatialTransform() {
    Node3D::invalidateSpatialTransform();
    syncData();
}

void Light3D::syncData() {
    light_data_.transform = getGlobalTransform();
    RenderingServer::get().lightSetData(light_rid_, light_data_);
}
