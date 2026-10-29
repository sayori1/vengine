#pragma once

#include "Material.h"
#include "Texture2D.h"

#include <array>

class StandardMaterial3D : public Material {
public:
    enum class TextureSlot { Albedo, Normal, MetallicRoughness, AmbientOcclusion, Emissive, Count };

    static Ref<StandardMaterial3D> create();

    void setBaseColor(const glm::vec4& color);
    glm::vec4 getBaseColor() const;
    void setMetallic(float value);
    float getMetallic() const;
    void setRoughness(float value);
    float getRoughness() const;
    void setNormalScale(float value);
    float getNormalScale() const;
    void setEmission(const glm::vec3& color);
    glm::vec3 getEmission() const;
    bool setTexture(TextureSlot slot, Ref<Texture2D> texture);
    const Ref<Texture2D>& getTexture(TextureSlot slot) const;

private:
    StandardMaterial3D();

    std::array<Ref<Texture2D>, static_cast<size_t>(TextureSlot::Count)> textures_{};
};
