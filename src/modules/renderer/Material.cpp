#include "Material.h"

#include <utility>

Material::Material(Ref<Shader> shader)
    : shader_(std::move(shader)) {}

Ref<Material> Material::create(Ref<Shader> shader) {
    return Ref<Material>(
        new Material(std::move(shader))
    );
}

void Material::setShader(Ref<Shader> shader) {
    shader_ = std::move(shader);
    ++version_;
}

const Ref<Shader>& Material::getShader() const {
    return shader_;
}

void Material::setParameter(
    std::string name,
    Parameter value
) {
    parameters_.insert_or_assign(
        std::move(name),
        std::move(value)
    );

    ++version_;
}

const Material::Parameter* Material::getParameter(
    std::string_view name
) const {
    auto it = parameters_.find(std::string(name));

    if (it == parameters_.end()) {
        return nullptr;
    }

    return &it->second;
}

bool Material::removeParameter(std::string_view name) {
    if (parameters_.erase(std::string(name)) == 0) {
        return false;
    }

    ++version_;
    return true;
}

void Material::clearParameters() {
    if (parameters_.empty()) {
        return;
    }

    parameters_.clear();
    ++version_;
}

const std::unordered_map<std::string, Material::Parameter>&
Material::getParameters() const {
    return parameters_;
}

uint64_t Material::getVersion() const {
    return version_;
}
