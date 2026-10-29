#pragma once

#include "Node3D.h"
#include "modules/renderer/RID.h"

#include <glm/glm.hpp>

class Camera3D : public Node3D {
public:
    enum class ProjectionMode {
        Perspective,
        Orthographic
    };

    explicit Camera3D(std::string name = "Camera3D");
    ~Camera3D() override;

    void makeCurrent();

    void setProjectionMode(ProjectionMode mode);
    ProjectionMode getProjectionMode() const;

    void setFov(float degrees);
    float getFov() const;

    void setNear(float value);
    float getNear() const;

    void setFar(float value);
    float getFar() const;

    void setAspectRatio(float ratio);
    float getAspectRatio() const;

    void setOrthographicSize(float size);
    float getOrthographicSize() const;

    const glm::mat4& getViewMatrix() const;
    const glm::mat4& getProjectionMatrix() const;
    const glm::mat4& getViewProjectionMatrix() const;

protected:
    void invalidateSpatialTransform() override;

private:
    void invalidateProjection();
    void updateViewMatrix() const;
    void updateProjectionMatrix() const;

    void syncTransform();
    void syncProjection();

    RID cameraRID_;

    ProjectionMode projectionMode_ = ProjectionMode::Perspective;

    float fov_ = 75.0f;
    float near_ = 0.05f;
    float far_ = 4000.0f;
    float aspectRatio_ = 16.0f / 9.0f;
    float orthographicSize_ = 10.0f;

    mutable glm::mat4 viewMatrix_{1.0f};
    mutable glm::mat4 projectionMatrix_{1.0f};
    mutable glm::mat4 viewProjectionMatrix_{1.0f};

    mutable bool viewDirty_ = true;
    mutable bool projectionDirty_ = true;
    mutable bool viewProjectionDirty_ = true;
};
