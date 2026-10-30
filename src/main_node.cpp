#include "main_node.h"

#include "modules/node/Camera3D.h"
#include "modules/node/DirectionalLight3D.h"
#include "modules/node/MeshInstance3D.h"
#include "modules/node/PointLight3D.h"
#include "modules/node/SpotLight3D.h"
#include "modules/renderer/Mesh.h"
#include "modules/renderer/StandardMaterial3D.h"
#include "modules/ui/Panel.h"
#include "modules/ui/Padding.h"
#include "modules/ui/Row.h"
#include "modules/ui/Column.h"
#include "modules/ui/Expanded.h"
#include "modules/ui/Stack.h"
#include "modules/ui/Label.h"
#include "modules/renderer/RenderingServer.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace {

Ref<Mesh> createCube(const Ref<StandardMaterial3D>& material) {
    const std::array<glm::vec3, 6> normals = {{
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
        {0, -1, 0}, {0, 0, 1}, {0, 0, -1}
    }};
    const std::array<glm::vec3, 6> tangents = {{
        {0, 0, -1}, {0, 0, 1}, {1, 0, 0},
        {1, 0, 0}, {1, 0, 0}, {-1, 0, 0}
    }};
    const std::array<glm::vec2, 4> coordinates = {{{0, 0}, {1, 0}, {1, 1}, {0, 1}}};
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    vertices.reserve(24);
    indices.reserve(36);
    for (size_t face = 0; face < normals.size(); ++face) {
        const glm::vec3 normal = normals[face];
        const glm::vec3 tangent = tangents[face];
        const glm::vec3 bitangent = glm::cross(normal, tangent);
        const uint32_t base = static_cast<uint32_t>(vertices.size());
        for (const auto& uv : coordinates) {
            Vertex vertex;
            vertex.position = normal * 0.5f + tangent * (uv.x - 0.5f) + bitangent * (uv.y - 0.5f);
            vertex.normal = normal;
            vertex.tangent = glm::vec4(tangent, 1.0f);
            vertex.uv = uv;
            vertices.push_back(vertex);
        }
        indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }
    auto mesh = Mesh::create();
    if (!mesh || !mesh->addSurface(vertices, indices)) {
        throw std::runtime_error("MainNode: cube mesh creation failed");
    }
    mesh->getSurface(0).material = material;
    return mesh;
}

Ref<StandardMaterial3D> createTexturedMaterial() {
    auto material = StandardMaterial3D::create();
    material->setMetallic(0.8f);
    material->setRoughness(0.7f);
    material->setNormalScale(0.5f);
    material->setEmission(glm::vec3(0.08f));
    constexpr int kSize = 32;
    using Slot = StandardMaterial3D::TextureSlot;
    for (size_t index = 0; index < static_cast<size_t>(Slot::Count); ++index) {
        const auto slot = static_cast<Slot>(index);
        std::vector<uint8_t> pixels(kSize * kSize * 4);
        for (int y = 0; y < kSize; ++y) {
            for (int x = 0; x < kSize; ++x) {
                const bool alternate = ((x / 8 + y / 8) % 2) != 0;
                std::array<uint8_t, 4> color{255, 255, 255, 255};
                switch (slot) {
                case Slot::Albedo:
                    color = alternate ? std::array<uint8_t, 4>{35, 110, 210, 255}
                                      : std::array<uint8_t, 4>{220, 160, 65, 255};
                    break;
                case Slot::Normal:
                    color = {static_cast<uint8_t>(alternate ? 140 : 116), 128, 254, 255};
                    break;
                case Slot::MetallicRoughness:
                    color = {255, static_cast<uint8_t>(alternate ? 90 : 210), 230, 255};
                    break;
                case Slot::AmbientOcclusion:
                    color = {230, 230, 230, 255};
                    break;
                case Slot::Emissive:
                    color = alternate ? std::array<uint8_t, 4>{20, 60, 150, 255}
                                      : std::array<uint8_t, 4>{0, 0, 0, 255};
                    break;
                case Slot::Count:
                    break;
                }
                const size_t offset = static_cast<size_t>(y * kSize + x) * 4;
                for (size_t channel = 0; channel < color.size(); ++channel) {
                    pixels[offset + channel] = color[channel];
                }
            }
        }
        const auto colorSpace = slot == Slot::Albedo || slot == Slot::Emissive
            ? Texture2D::ColorSpace::Srgb : Texture2D::ColorSpace::Linear;
        auto texture = Texture2D::create(kSize, kSize, pixels, colorSpace);
        if (!texture || !material->setTexture(slot, texture)) {
            throw std::runtime_error("MainNode: procedural material texture creation failed");
        }
    }
    return material;
}

void aimAt(Node3D& node, const glm::vec3& target) {
    node.setRotation(glm::quat(glm::vec3(1, 0, 0), glm::normalize(target - node.getPosition())));
}

}

MainNode::MainNode() : Node3D("MainNode") {
    auto& panel = addChild<Panel>("DemoPanel");
    panel.setPosition({24.0f, 24.0f});
    panel.setSize({360.0f, 120.0f});
    panel.setMinimumSize({240.0f, 80.0f});
    panel.setBackgroundColor({0.08f, 0.18f, 0.28f, 0.82f});
    panel.setPadding({12.0f, 12.0f,12.0f,12.0f});

    auto& column = panel.addChild<Column>("DemoColumn");

    auto& label = column.addChild<Label>("UnicodeLabel");
    label.setText("· Привет 1");
    label.setFontSize(18.0f);
    label.setColor({1.0f, 1.0f, 1.0f, 1.0f});


    auto& label1 = column.addChild<Label>("UnicodeLabel1");
    label1.setText("· Привет 2");
    label1.setFontSize(24.0f);
    label1.setColor({1.0f, 1.0f, 1.0f, 1.0f});

    auto& camera = addChild<Camera3D>("Camera");
    camera.setPosition({-5.0f, 3.0f, -4.0f});
    aimAt(camera, {0.0f, 0.5f, 0.0f});
    camera.setFov(65.0f);
    camera.makeCurrent();

    auto& cube = addChild<MeshInstance3D>("Cube");
    cube.setMesh(createCube(createTexturedMaterial()));
    cube.setPosition({0.0f, 0.8f, 0.0f});
    cube.setScale(glm::vec3(1.5f));
    cube_ = &cube;

    auto groundMaterial = StandardMaterial3D::create();
    groundMaterial->setBaseColor({0.42f, 0.45f, 0.48f, 1.0f});
    groundMaterial->setRoughness(0.9f);
    auto& ground = addChild<MeshInstance3D>("Ground");
    ground.setMesh(createCube(groundMaterial));
    ground.setPosition({0.0f, -0.15f, 0.0f});
    ground.setScale({14.0f, 0.2f, 14.0f});

    auto& sun = addChild<DirectionalLight3D>("Sun");
    sun.setPosition({-4.0f, 7.0f, -3.0f});
    aimAt(sun, glm::vec3(0.0f));
    sun.setColor({1.0f, 0.92f, 0.8f});
    sun.setIntensity(2.0f);
    sun.setShadowsEnabled(true);

    auto& point = addChild<PointLight3D>("MovingPoint");
    point.setPosition({-2.0f, 2.0f, 0.0f});
    point.setColor({0.25f, 0.5f, 1.0f});
    point.setIntensity(25.0f);
    point.setRadius(9.0f);
    point.setShadowsEnabled(true);
    moving_light_ = &point;

    auto& spot = addChild<SpotLight3D>("Spot");
    spot.setPosition({2.0f, 4.0f, -3.0f});
    aimAt(spot, {0.0f, 0.0f, 0.0f});
    spot.setColor({1.0f, 0.4f, 0.18f});
    spot.setIntensity(45.0f);
    spot.setRadius(12.0f);
    spot.setOuterAngle(35.0f);
    spot.setInnerAngle(22.0f);
    spot.setShadowsEnabled(true);
}

void MainNode::onUpdate(float dt) {
    if (!std::isfinite(dt) || dt < 0.0f) {
        return;
    }
    elapsed_ = std::fmod(elapsed_ + dt, 100.0f * glm::pi<float>());
    cube_->setRotation(glm::angleAxis(elapsed_ * 0.4f, glm::vec3(0, 1, 0)));
    moving_light_->setPosition({3.0f * std::cos(elapsed_), 2.5f, 3.0f * std::sin(elapsed_)});
}
