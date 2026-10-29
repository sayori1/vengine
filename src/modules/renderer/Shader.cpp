#include "Shader.h"

#include <utility>

Shader::Shader(
    std::string vertexSource,
    std::string fragmentSource
)
    : vertexSource_(std::move(vertexSource)),
      fragmentSource_(std::move(fragmentSource)) {}

Ref<Shader> Shader::create(
    std::string vertexSource,
    std::string fragmentSource
) {
    return Ref<Shader>(
        new Shader(
            std::move(vertexSource),
            std::move(fragmentSource)
        )
    );
}

const std::string& Shader::getVertexSource() const {
    return vertexSource_;
}

const std::string& Shader::getFragmentSource() const {
    return fragmentSource_;
}

void Shader::setSource(
    std::string vertexSource,
    std::string fragmentSource
) {
    vertexSource_ = std::move(vertexSource);
    fragmentSource_ = std::move(fragmentSource);

    ++version_;
}

uint64_t Shader::getVersion() const {
    return version_;
}
