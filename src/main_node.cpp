#include "main_node.h"

#include "modules/node/Camera3D.h"
#include "modules/node/MeshInstance3D.h"

#include "modules/renderer/Material.h"
#include "modules/renderer/Mesh.h"
#include "modules/renderer/Shader.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace {

const std::string vertexSource = R"(
#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;

uniform mat4 u_mvp;

out vec4 v_color;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_color = a_color;
}
)";

const std::string fragmentSource = R"(
#version 330 core

in vec4 v_color;

uniform vec4 u_color;

out vec4 frag_color;

void main() {
    frag_color = v_color * u_color;
}
)";

Ref<Mesh> createCube() {
    std::vector<Vertex> vertices(8);

    vertices[0].position = {-0.5f, -0.5f, -0.5f};
    vertices[1].position = { 0.5f, -0.5f, -0.5f};
    vertices[2].position = { 0.5f,  0.5f, -0.5f};
    vertices[3].position = {-0.5f,  0.5f, -0.5f};

    vertices[4].position = {-0.5f, -0.5f,  0.5f};
    vertices[5].position = { 0.5f, -0.5f,  0.5f};
    vertices[6].position = { 0.5f,  0.5f,  0.5f};
    vertices[7].position = {-0.5f,  0.5f,  0.5f};

    const std::vector<uint32_t> indices = {
        0, 2, 1, 0, 3, 2,
        4, 5, 6, 4, 6, 7,
        0, 1, 5, 0, 5, 4,
        2, 3, 7, 2, 7, 6,
        1, 2, 6, 1, 6, 5,
        3, 0, 4, 3, 4, 7
    };

    auto mesh = Mesh::create();

    if (!mesh || !mesh->addSurface(vertices, indices)) {
        return nullptr;
    }

    auto shader = Shader::create(
        vertexSource,
        fragmentSource
    );

    auto material = Material::create(shader);

    material->setParameter(
        "albedo_color",
        glm::vec4(0.3f, 0.7f, 1.0f, 1.0f)
    );

    mesh->getSurface(0).material = material;

    return mesh;
}

}

MainNode::MainNode()
    : Node3D("MainNode") {

    auto& camera = addChild<Camera3D>("Camera");

    camera.setPosition({-4.0f, 2.0f, 0.0f});

    const glm::vec3 direction = glm::normalize(
        glm::vec3(4.0f, -2.0f, 0.0f)
    );

    const float pitch = glm::atan(
        direction.y,
        direction.x
    );

    camera.setRotation(
        glm::angleAxis(
            pitch,
            glm::vec3(0.0f, 0.0f, 1.0f)
        )
    );

    camera.setFov(75.0f);
    camera.makeCurrent();

    auto& cube = addChild<MeshInstance3D>("Cube");

    cube.setMesh(createCube());
    cube.setPosition({0.0f, 0.0f, 0.0f});
    cube.getMesh()->getSurface(0).material->setParameter(
        "albedo_color",
        glm::vec4(0.5f, 0.3f, 0.7f, 1.0f)
    );
}
