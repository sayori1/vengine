#pragma once

#include "Node.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class Node3D : public Node {
public:
    explicit Node3D(std::string name = {});
    ~Node3D() override;

    const glm::vec3& getPosition() const;
        glm::vec3 getGlobalPosition() const;
    const glm::quat& getRotation() const;
    const glm::vec3& getScale() const;
    void setPosition(const glm::vec3& position);
    void setRotation(const glm::quat& rotation);
    void setScale(const glm::vec3& scale);
    const glm::mat4& getLocalTransform() const;
    const glm::mat4& getGlobalTransform() const;

protected:
    const Node3D* spatialNode() const override;
    void invalidateSpatialTransform() override;

private:
    glm::vec3 position_{0.0f};
    glm::quat rotation_{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale_{1.0f};
    mutable glm::mat4 local_transform_{1.0f};
    mutable glm::mat4 global_transform_{1.0f};
    mutable bool local_dirty_ = true;
    mutable bool global_dirty_ = true;
};
