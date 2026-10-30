#pragma once

#include "modules/node/Node.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <limits>

class UIServer;

struct LayoutConstraints {
    glm::vec2 minimum{0.0f};
    glm::vec2 maximum{0.0f};
};

struct EdgeInsets {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
};

class Control : public Node {
public:
    using Node::Node;
    ~Control() override = default;

    void setPosition(const glm::vec2& position);
    void setSize(const glm::vec2& size);
    void setVisible(bool visible);
    void setZIndex(int zIndex);
    void setPadding(const EdgeInsets& padding);
    const EdgeInsets& getPadding() const;
    void setMinimumSize(const glm::vec2& size);
    void setMaximumSize(const glm::vec2& size);
    glm::vec2 measure(const LayoutConstraints& constraints) const;

    const glm::vec2& getPosition() const;
    const glm::vec2& getSize() const;
    bool isVisible() const;
    int getZIndex() const;
    glm::vec2 getGlobalPosition() const;

protected:
    void onReady() override;
    void onDestroy() override;
    virtual void onDraw(UIServer& uiServer, const glm::vec2& globalPosition);
    virtual void onLayout(UIServer& uiServer, const glm::vec2& availableSize);
    virtual glm::vec2 measure(UIServer& uiServer, const LayoutConstraints& constraints);
    virtual void layoutChildren(UIServer& uiServer, const glm::vec2& availableSize);
    void markLayoutDirty();
    void setLayoutSize(const glm::vec2& size);
    glm::vec2 constrainSize(const glm::vec2& preferred,
                            const LayoutConstraints& constraints) const;
    int getDrawZIndex() const;
    bool hasExplicitSize() const;

private:
    friend class UIServer;
    friend class Row;
    friend class Column;
    friend class Stack;
    friend class Padding;
    friend class Expanded;
    glm::vec2 position_{0.0f};
    glm::vec2 size_{0.0f};
    glm::vec2 global_position_{0.0f};
    glm::vec2 last_available_size_{-1.0f};
    glm::vec2 minimum_size_{0.0f};
    glm::vec2 maximum_size_{std::numeric_limits<float>::max()};
    EdgeInsets padding_{};
    bool visible_ = true;
    bool layout_dirty_ = true;
    bool explicit_size_ = false;
    int z_index_ = 0;
    int draw_z_index_ = 0;
};
