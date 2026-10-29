#version 430 core

in vec2 v_uv;
layout(location = 0) out vec4 frag_color;
uniform sampler2D u_hdr_texture;
uniform vec4 u_exposure;

vec3 linearToSrgb(vec3 color) {
    return mix(1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055,
               12.92 * color, lessThanEqual(color, vec3(0.0031308)));
}

void main() {
    vec3 hdr = max(texture(u_hdr_texture, v_uv).rgb * u_exposure.x, vec3(0.0));
    vec3 mapped = clamp((hdr * (2.51 * hdr + 0.03)) /
                        (hdr * (2.43 * hdr + 0.59) + 0.14), 0.0, 1.0);
    frag_color = vec4(u_exposure.y > 0.5 ? mapped : linearToSrgb(mapped), 1.0);
}
