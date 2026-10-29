#pragma once

#include "modules/resource/Resource.h"
#include "Shader.h"

#include <glm/glm.hpp>

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

class Material : public Resource {
public:
    using Parameter = std::variant<
        bool,
        int,
        float,
        glm::vec2,
        glm::vec3,
        glm::vec4,
        glm::mat4
    >;

    static Ref<Material> create(Ref<Shader> shader = nullptr);

    void setShader(Ref<Shader> shader);
    const Ref<Shader>& getShader() const;

    void setParameter(
        std::string name,
        Parameter value
    );

    const Parameter* getParameter(std::string_view name) const;

    bool removeParameter(std::string_view name);
    void clearParameters();

    const std::unordered_map<std::string, Parameter>&
    getParameters() const;

    uint64_t getVersion() const;

protected:
    explicit Material(Ref<Shader> shader);

private:
    Ref<Shader> shader_;

    std::unordered_map<std::string, Parameter> parameters_;

    uint64_t version_ = 1;
};
