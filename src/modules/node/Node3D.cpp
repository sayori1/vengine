#include "Node3D.h"

#include <cassert>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

Node3D::Node3D(std::string name) : Node(std::move(name)) {}
Node3D::~Node3D() = default;

const glm::vec3& Node3D::getPosition() const { return position_; }
glm::vec3 Node3D::getGlobalPosition() const { return glm::vec3(getGlobalTransform()[3]); }
const glm::quat& Node3D::getRotation() const { return rotation_; }
const glm::vec3& Node3D::getScale() const { return scale_; }

void Node3D::setPosition(const glm::vec3& position) {
    position_ = position;
    local_dirty_ = true;
    invalidateTransforms();
}

void Node3D::setRotation(const glm::quat& rotation) {
    const float length = glm::length(rotation);
    assert(std::isfinite(length) && length > 0.0f);
    if (!std::isfinite(length) || length <= 0.0f) {
        return;
    }
    rotation_ = glm::normalize(rotation);
    local_dirty_ = true;
    invalidateTransforms();
}

void Node3D::setScale(const glm::vec3& scale) {
    scale_ = scale;
    local_dirty_ = true;
    invalidateTransforms();
}

const glm::mat4& Node3D::getLocalTransform() const {
    if (local_dirty_) {
        local_transform_ = glm::translate(glm::mat4(1.0f), position_)
            * glm::mat4_cast(rotation_)
            * glm::scale(glm::mat4(1.0f), scale_);
        local_dirty_ = false;
    }
    return local_transform_;
}

const glm::mat4& Node3D::getGlobalTransform() const {
    if (global_dirty_) {
        const Node3D* ancestor = spatialAncestor();
        global_transform_ = ancestor
            ? ancestor->getGlobalTransform() * getLocalTransform()
            : getLocalTransform();
        global_dirty_ = false;
    }
    return global_transform_;
}

const Node3D* Node3D::spatialNode() const { return this; }
void Node3D::invalidateSpatialTransform() { global_dirty_ = true; }
