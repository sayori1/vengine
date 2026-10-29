#pragma once

#include "Node3D.h"
#include "modules/renderer/LightData.h"
#include "modules/renderer/RID.h"

class Light3D : public Node3D {
public:
    ~Light3D() override;

    void setColor(const glm::vec3& color);
    const glm::vec3& getColor() const;
    void setIntensity(float intensity);
    float getIntensity() const;
    void setRadius(float radius);
    float getRadius() const;
    void setInnerAngle(float degrees);
    float getInnerAngle() const;
    void setOuterAngle(float degrees);
    float getOuterAngle() const;
    void setShadowsEnabled(bool enabled);
    bool getShadowsEnabled() const;
    LightType getLightType() const;
    RID getRID() const;

protected:
    Light3D(LightType type, std::string name);
    void invalidateSpatialTransform() override;

private:
    void syncData();

    RID light_rid_;
    LightData light_data_;
};
