#include "StandardMaterial3D.h"

#include "GeometryShaderSources.h"

#include <cassert>
#include <cmath>
#include <utility>

StandardMaterial3D::StandardMaterial3D()
    : Material(Shader::create(geometry_shaders::kPbrVertexSource, geometry_shaders::kPbrFragmentSource)) {
    setBaseColor(glm::vec4(1.0f));
    setMetallic(0.0f);
    setRoughness(0.5f);
    setNormalScale(1.0f);
    setEmission(glm::vec3(0.0f));
}

Ref<StandardMaterial3D> StandardMaterial3D::create() {
    return Ref<StandardMaterial3D>(new StandardMaterial3D());
}

void StandardMaterial3D::setBaseColor(const glm::vec4& color) {
    if (!std::isfinite(color.x) || !std::isfinite(color.y) || !std::isfinite(color.z) ||
        !std::isfinite(color.w) || glm::any(glm::lessThan(color, glm::vec4(0.0f))) ||
        glm::any(glm::greaterThan(color, glm::vec4(1.0f)))) {
        return;
    }
    setParameter("albedo_color", color);
}

glm::vec4 StandardMaterial3D::getBaseColor() const {
    const auto* value = getParameter("albedo_color");
    return value && std::holds_alternative<glm::vec4>(*value) ? std::get<glm::vec4>(*value) : glm::vec4(1.0f);
}

void StandardMaterial3D::setMetallic(float value) {
    if (std::isfinite(value) && value >= 0.0f && value <= 1.0f) {
        setParameter("metallic", value);
    }
}

float StandardMaterial3D::getMetallic() const {
    const auto* value = getParameter("metallic");
    return value && std::holds_alternative<float>(*value) ? std::get<float>(*value) : 0.0f;
}

void StandardMaterial3D::setRoughness(float value) {
    if (std::isfinite(value) && value >= 0.0f && value <= 1.0f) {
        setParameter("roughness", value);
    }
}

float StandardMaterial3D::getRoughness() const {
    const auto* value = getParameter("roughness");
    return value && std::holds_alternative<float>(*value) ? std::get<float>(*value) : 0.5f;
}

void StandardMaterial3D::setNormalScale(float value) {
    if (std::isfinite(value) && value >= 0.0f) {
        setParameter("normal_scale", value);
    }
}

float StandardMaterial3D::getNormalScale() const {
    const auto* value = getParameter("normal_scale");
    return value && std::holds_alternative<float>(*value) ? std::get<float>(*value) : 1.0f;
}

void StandardMaterial3D::setEmission(const glm::vec3& color) {
    if (std::isfinite(color.x) && std::isfinite(color.y) && std::isfinite(color.z) &&
        glm::all(glm::greaterThanEqual(color, glm::vec3(0.0f)))) {
        setParameter("emission", color);
    }
}

glm::vec3 StandardMaterial3D::getEmission() const {
    const auto* value = getParameter("emission");
    return value && std::holds_alternative<glm::vec3>(*value) ? std::get<glm::vec3>(*value) : glm::vec3(0.0f);
}

bool StandardMaterial3D::setTexture(TextureSlot slot, Ref<Texture2D> texture) {
    const size_t index = static_cast<size_t>(slot);
    if (index >= textures_.size()) {
        return false;
    }
    const auto colorSpace = slot == TextureSlot::Albedo || slot == TextureSlot::Emissive
        ? Texture2D::ColorSpace::Srgb : Texture2D::ColorSpace::Linear;
    if (texture && texture->getColorSpace() != colorSpace) {
        return false;
    }
    textures_[index] = std::move(texture);
    return true;
}

const Ref<Texture2D>& StandardMaterial3D::getTexture(TextureSlot slot) const {
    const size_t index = static_cast<size_t>(slot);
    assert(index < textures_.size());
    return textures_.at(index);
}
