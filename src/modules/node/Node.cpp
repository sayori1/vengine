#include "Node.h"
#include "modules/ui/UIServer.h"

#include <cassert>

Node::Node(std::string name) : name_(std::move(name)) {}

Node::~Node() {
    assert(destroyed_ && "Call destroy() before the most-derived root destructor");
}

bool Node::removeChild(Node* child) {
    if (!child || child->parent_ != this || child->removal_requested_) {
        return false;
    }
    child->removal_requested_ = true;
    requestFlush();
    return true;
}

Node* Node::findChild(std::string_view name) {
    return const_cast<Node*>(std::as_const(*this).findChild(name));
}

const Node* Node::findChild(std::string_view name) const {
    for (const auto* collection : {&children_, &pending_children_}) {
        for (const auto& child : *collection) {
            if (!child->isRetiring() && child->name_ == name) {
                return child.get();
            }
        }
    }
    return nullptr;
}

Node* Node::getParent() { return parent_; }
const Node* Node::getParent() const { return parent_; }
const std::vector<std::unique_ptr<Node>>& Node::getChildren() const { return children_; }
const std::string& Node::getName() const { return name_; }
void Node::setName(std::string name) { name_ = std::move(name); }

Node* Node::root() {
    Node* node = this;
    while (node->parent_) {
        node = node->parent_;
    }
    return node;
}

bool Node::isRetiring() const {
    for (const Node* node = this; node; node = node->parent_) {
        if (node->removal_requested_ || node->destroy_requested_ || node->destroying_ || node->destroyed_) {
            return true;
        }
    }
    return false;
}

void Node::requestFlush() {
    Node* treeRoot = root();
    treeRoot->flush_requested_ = true;
    if (!treeRoot->traversing_) {
        treeRoot->traversing_ = true;
        treeRoot->flush();
        treeRoot->traversing_ = false;
    }
}

void Node::update(float dt) {
    assert(!parent_ && "Only the application-owned root may drive update()");
    if (parent_ || traversing_ || destroyed_) {
        return;
    }
    traversing_ = true;
    active_ = true;
    flush_requested_ = true;
    flush();
    if (!isRetiring()) {
        updateSubtree(dt);
    }
    flush();
    traversing_ = false;
}

void Node::update(float dt, UIServer& uiServer) {
    assignUIServer(&uiServer);
    update(dt);
}

void Node::assignUIServer(UIServer* uiServer) {
    ui_server_ = uiServer;
    for (auto* collection : {&children_, &pending_children_}) {
        for (auto& child : *collection) {
            child->assignUIServer(uiServer);
        }
    }
}

void Node::destroy() {
    if (destroyed_ || destroying_) {
        return;
    }
    Node* treeRoot = root();
    destroy_requested_ = true;
    treeRoot->flush_requested_ = true;
    if (treeRoot->traversing_) {
        return;
    }
    treeRoot->traversing_ = true;
    treeRoot->flush();
    treeRoot->traversing_ = false;
}

void Node::flush() {
    while (flush_requested_) {
        flush_requested_ = false;
        settle(active_);
    }
}

void Node::settle(bool active) {
    if (destroy_requested_) {
        destroySubtree();
        return;
    }
    if (destroyed_ || isRetiring()) {
        return;
    }
    for (auto& child : pending_children_) {
        child->ui_server_ = ui_server_;
        children_.push_back(std::move(child));
    }
    pending_children_.clear();
    if (active && !ready_called_) {
        ready_called_ = true;
        onReady();
    }
    for (auto iterator = children_.begin(); iterator != children_.end();) {
        Node& child = **iterator;
        if (child.removal_requested_) {
            child.destroySubtree();
            iterator = children_.erase(iterator);
        } else {
            if (!isRetiring()) {
                child.settle(active && ready_called_);
            }
            ++iterator;
        }
    }
}

void Node::updateSubtree(float dt) {
    if (isRetiring() || !ready_called_) {
        return;
    }
    onUpdate(dt);
    for (const auto& child : children_) {
        if (isRetiring()) {
            return;
        }
        child->updateSubtree(dt);
    }
}

void Node::destroySubtree() {
    if (destroyed_ || destroying_) {
        return;
    }
    destroying_ = true;
    for (auto* collection : {&children_, &pending_children_}) {
        for (auto& child : *collection) {
            child->destroySubtree();
        }
        collection->clear();
    }
    onDestroy();
    destroyed_ = true;
    destroying_ = false;
}

void Node::onReady() {}
void Node::onUpdate(float) {}
void Node::onDestroy() {}
const Node3D* Node::spatialNode() const { return nullptr; }
void Node::invalidateSpatialTransform() {}

const Node3D* Node::spatialAncestor() const {
    for (const Node* node = parent_; node; node = node->parent_) {
        if (const Node3D* spatial = node->spatialNode()) {
            return spatial;
        }
    }
    return nullptr;
}

UIServer* Node::getUIServer() const { return ui_server_; }

void Node::invalidateTransforms() {
    invalidateSpatialTransform();
    for (auto* collection : {&children_, &pending_children_}) {
        for (auto& child : *collection) {
            child->invalidateTransforms();
        }
    }
}
