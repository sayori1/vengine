#include "Camera3D.h"

#include "modules/renderer/RenderingServer.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <cmath>
#include <utility>

Camera3D::Camera3D(std::string name)
    : Node3D(std::move(name)) {

    cameraRID_ = RenderingServer::get().cameraCreate();

    syncTransform();
    syncProjection();
}

Camera3D::~Camera3D() {
    RenderingServer::get().cameraFree(cameraRID_);
}

void Camera3D::makeCurrent() {
    RenderingServer::get().cameraSetActive(cameraRID_);
}

void Camera3D::setProjectionMode(ProjectionMode mode) {
    if (projectionMode_ == mode) {
        return;
    }

    projectionMode_ = mode;
    invalidateProjection();
}

Camera3D::ProjectionMode Camera3D::getProjectionMode() const {
    return projectionMode_;
}

void Camera3D::setFov(float degrees) {
    if (!std::isfinite(degrees) ||
        degrees <= 0.0f ||
        degrees >= 180.0f) {
        return;
    }

    fov_ = degrees;
    invalidateProjection();
}

float Camera3D::getFov() const {
    return fov_;
}

void Camera3D::setNear(float value) {
    if (!std::isfinite(value) ||
        value <= 0.0f ||
        value >= far_) {
        return;
    }

    near_ = value;
    invalidateProjection();
}

float Camera3D::getNear() const {
    return near_;
}

void Camera3D::setFar(float value) {
    if (!std::isfinite(value) || value <= near_) {
        return;
    }

    far_ = value;
    invalidateProjection();
}

float Camera3D::getFar() const {
    return far_;
}

void Camera3D::setAspectRatio(float ratio) {
    if (!std::isfinite(ratio) || ratio <= 0.0f) {
        return;
    }

    aspectRatio_ = ratio;
    invalidateProjection();
}

float Camera3D::getAspectRatio() const {
    return aspectRatio_;
}

void Camera3D::setOrthographicSize(float size) {
    if (!std::isfinite(size) || size <= 0.0f) {
        return;
    }

    orthographicSize_ = size;
    invalidateProjection();
}

float Camera3D::getOrthographicSize() const {
    return orthographicSize_;
}

const glm::mat4& Camera3D::getViewMatrix() const {
    if (viewDirty_) {
        updateViewMatrix();
    }

    return viewMatrix_;
}

const glm::mat4& Camera3D::getProjectionMatrix() const {
    if (projectionDirty_) {
        updateProjectionMatrix();
    }

    return projectionMatrix_;
}

const glm::mat4& Camera3D::getViewProjectionMatrix() const {
    if (viewProjectionDirty_) {
        viewProjectionMatrix_ =
            getProjectionMatrix() * getViewMatrix();

        viewProjectionDirty_ = false;
    }

    return viewProjectionMatrix_;
}

void Camera3D::invalidateSpatialTransform() {
    Node3D::invalidateSpatialTransform();

    viewDirty_ = true;
    viewProjectionDirty_ = true;

    syncTransform();
}

void Camera3D::invalidateProjection() {
    projectionDirty_ = true;
    viewProjectionDirty_ = true;

    syncProjection();
}

void Camera3D::updateViewMatrix() const {
    const glm::mat4& transform = getGlobalTransform();

    const glm::vec3 position = glm::vec3(transform[3]);

    const glm::vec3 forward = glm::normalize(
        glm::vec3(transform * glm::vec4(1, 0, 0, 0))
    );

    const glm::vec3 up = glm::normalize(
        glm::vec3(transform * glm::vec4(0, 1, 0, 0))
    );

    viewMatrix_ = glm::lookAtLH(
        position,
        position + forward,
        up
    );

    viewDirty_ = false;
}

void Camera3D::updateProjectionMatrix() const {
    if (projectionMode_ == ProjectionMode::Perspective) {
        projectionMatrix_ = glm::perspectiveLH_NO(
            glm::radians(fov_),
            aspectRatio_,
            near_,
            far_
        );
    } else {
        const float halfHeight = orthographicSize_ * 0.5f;
        const float halfWidth = halfHeight * aspectRatio_;

        projectionMatrix_ = glm::orthoLH_NO(
            -halfWidth,
            halfWidth,
            -halfHeight,
            halfHeight,
            near_,
            far_
        );
    }

    projectionDirty_ = false;
}

void Camera3D::syncTransform() {
    RenderingServer::get().cameraSetView(
        cameraRID_,
        getViewMatrix()
    );
}

void Camera3D::syncProjection() {
    RenderingServer::get().cameraSetProjection(
        cameraRID_,
        getProjectionMatrix()
    );
}
