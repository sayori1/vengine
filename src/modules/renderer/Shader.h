#pragma once

#include "modules/resource/Resource.h"

#include <string>

class Shader : public Resource {
public:
    static Ref<Shader> create(
        std::string vertexSource,
        std::string fragmentSource
    );

    const std::string& getVertexSource() const;
    const std::string& getFragmentSource() const;

    void setSource(
        std::string vertexSource,
        std::string fragmentSource
    );

    uint64_t getVersion() const;

private:
    Shader(
        std::string vertexSource,
        std::string fragmentSource
    );

    std::string vertexSource_;
    std::string fragmentSource_;

    uint64_t version_ = 1;
};
