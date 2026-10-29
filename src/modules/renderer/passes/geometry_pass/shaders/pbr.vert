#version 430 core

invariant gl_Position;

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec3 a_normal;
layout(location = 3) in vec2 a_uv;
layout(location = 4) in vec4 a_tangent;

uniform mat4 u_mvp;
uniform mat4 u_model;
uniform mat4 u_normal_matrix;

out vec3 v_world_position;
out vec3 v_normal;
out vec4 v_tangent;
out vec2 v_uv;
out vec4 v_color;

void main() {
    gl_Position = u_mvp * vec4(a_position, 1.0);
    v_world_position = (u_model * vec4(a_position, 1.0)).xyz;
    v_normal = normalize(mat3(u_normal_matrix) * a_normal);
    v_tangent = vec4(mat3(u_model) * a_tangent.xyz,
                     a_tangent.w * (determinant(mat3(u_model)) < 0.0 ? -1.0 : 1.0));
    v_uv = a_uv;
    v_color = a_color;
}
