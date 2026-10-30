#version 430
layout(location=0) in vec4 v_color;
layout(location=1) in vec2 v_uv;
layout(location=2) in float v_textured;
layout(location=0) out vec4 o_color;
layout(binding=0) uniform sampler2D u_atlas;
void main() { vec4 atlas = texture(u_atlas, v_uv); o_color = v_color * mix(vec4(1.0), atlas, v_textured); }
