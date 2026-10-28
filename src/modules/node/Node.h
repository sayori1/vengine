#pragma once

#include <cassert>
#include <concepts>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class Node3D;

class Node {
public:
    explicit Node(std::string name = {});
    virtual ~Node();

    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(Node&&) = delete;

    template <typename T, typename... Args>
        requires std::derived_from<T, Node> && std::constructible_from<T, Args...>
    T& addChild(Args&&... args) {
        assert(!isRetiring());
        if (isRetiring()) {
            std::terminate();
        }
        auto child = std::make_unique<T>(std::forward<Args>(args)...);
        T* result = child.get();
        pending_children_.push_back(std::move(child));
        Node* node = result;
        node->parent_ = this;
        node->invalidateTransforms();
        requestFlush();
        return *result;
    }

    bool removeChild(Node* child);
    Node* findChild(std::string_view name);
    const Node* findChild(std::string_view name) const;
    Node* getParent();
    const Node* getParent() const;
    const std::vector<std::unique_ptr<Node>>& getChildren() const;
        const std::string& getName() const;
    void setName(std::string name);
    void update(float dt);
    void destroy();

protected:
    virtual void onReady();
    virtual void onUpdate(float dt);
    virtual void onDestroy();
    virtual const Node3D* spatialNode() const;
    virtual void invalidateSpatialTransform();
    const Node3D* spatialAncestor() const;
    void invalidateTransforms();

private:
    Node* root();
    bool isRetiring() const;
    void requestFlush();
    void flush();
    void settle(bool active);
    void updateSubtree(float dt);
    void destroySubtree();

    std::string name_;
    Node* parent_ = nullptr;
    std::vector<std::unique_ptr<Node>> children_;
    std::vector<std::unique_ptr<Node>> pending_children_;
    bool ready_called_ = false;
    bool destroy_requested_ = false;
    bool destroying_ = false;
    bool destroyed_ = false;
    bool removal_requested_ = false;
    bool active_ = false;
    bool traversing_ = false;
    bool flush_requested_ = false;
};
