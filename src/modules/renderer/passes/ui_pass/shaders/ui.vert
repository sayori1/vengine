#version 430
layout(location=0) in vec2 a_position;
layout(location=1) in vec4 a_color;
layout(location=2) in vec2 a_uv;
layout(location=3) in float a_textured;
layout(location=0) out vec4 v_color;
layout(location=1) out vec2 v_uv;
layout(location=2) out float v_textured;
void main() { gl_Position = vec4(a_position, 0.0, 1.0); v_color = a_color; v_uv = a_uv; v_textured = a_textured; }
