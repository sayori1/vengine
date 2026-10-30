#include "Font.h"

#include "modules/resource/ResourceManager.h"

#include <fstream>
#include <iterator>

Ref<Font> Font::load(const std::string& path) {
    ResourceManager::registerLoader<Font>([](const std::string& source) {
        std::ifstream file(source, std::ios::binary);
        if (!file) return Ref<Font>{};
        std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(file), {});
        return create(std::move(bytes));
    });
    return ResourceManager::load<Font>(path);
}

Ref<Font> Font::create(std::vector<uint8_t> data) {
    if (data.empty()) return nullptr;
    auto font = std::make_shared<Font>();
    font->data_ = std::move(data);
    return font;
}

const std::vector<uint8_t>& Font::getData() const { return data_; }

void Font::addFallback(const Ref<Font>& font) {
    if (font && font.get() != this) fallbacks_.push_back(font);
}

const std::vector<Ref<Font>>& Font::getFallbacks() const { return fallbacks_; }
