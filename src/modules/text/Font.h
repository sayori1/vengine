#pragma once

#include "modules/resource/Resource.h"

#include <cstdint>
#include <string>
#include <vector>

class Font : public Resource {
public:
    static Ref<Font> load(const std::string& path);
    static Ref<Font> create(std::vector<uint8_t> data);
    const std::vector<uint8_t>& getData() const;
    void addFallback(const Ref<Font>& font);
    const std::vector<Ref<Font>>& getFallbacks() const;

private:
    std::vector<uint8_t> data_;
    std::vector<Ref<Font>> fallbacks_;
};
