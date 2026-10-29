#version 430 core

in vec3 v_world_position;
in vec3 v_normal;
in vec4 v_tangent;
in vec2 v_uv;
in vec4 v_color;
layout(location = 0) out vec4 frag_color;

uniform mat4 u_view;
uniform vec4 u_base_color;
uniform vec4 u_material;
uniform vec4 u_emission;
uniform vec4 u_camera;
uniform vec4 u_ambient;
uniform vec4 u_screen;
uniform vec4 u_grid;
uniform vec4 u_z_parameters;
uniform vec4 u_shadow_texel;
uniform sampler2D u_albedo_texture;
uniform sampler2D u_normal_texture;
uniform sampler2D u_metallic_roughness_texture;
uniform sampler2D u_ao_texture;
uniform sampler2D u_emissive_texture;
uniform sampler2D u_shadow_atlas;

struct GPULight {
    vec4 positionRadius;
    vec4 directionOuter;
    vec4 colorIntensity;
    vec4 parameters;
    uvec4 metadata;
};
struct GPUShadow {
    mat4 viewProjection;
    vec4 atlasRect;
    vec4 parameters;
};
layout(std430, binding = 0) readonly buffer LightBuffer { GPULight lights[]; };
layout(std430, binding = 1) readonly buffer ClusterBuffer { uvec4 clusters[]; };
layout(std430, binding = 2) readonly buffer IndexBuffer { uint indices[]; };
layout(std430, binding = 3) readonly buffer ShadowBuffer { GPUShadow shadows[]; };

const float kPi = 3.141592653589793;

vec3 safeNormalize(vec3 value, vec3 fallback) {
    float lengthSquared = dot(value, value);
    return lengthSquared > 1e-12 ? value * inversesqrt(lengthSquared) : fallback;
}

float sampleShadow(uint index, vec3 normal) {
    if (index >= uint(shadows.length())) {
        return 1.0;
    }
    GPUShadow shadow = shadows[index];
    vec3 position = v_world_position + normal * shadow.parameters.z;
    vec4 clip = shadow.viewProjection * vec4(position, 1.0);
    if (clip.w <= 0.0) {
        return 1.0;
    }
    vec3 projected = clip.xyz / clip.w * 0.5 + 0.5;
    if (any(lessThan(projected, vec3(0.0))) || any(greaterThan(projected, vec3(1.0)))) {
        return 1.0;
    }
    vec2 minimum = shadow.atlasRect.xy + u_shadow_texel.xy * 0.5;
    vec2 maximum = shadow.atlasRect.xy + shadow.atlasRect.zw - u_shadow_texel.xy * 0.5;
    vec2 uv = shadow.atlasRect.xy + projected.xy * shadow.atlasRect.zw;
    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 tap = clamp(uv + vec2(x, y) * u_shadow_texel.xy, minimum, maximum);
            float depth = texture(u_shadow_atlas, tap).r;
            visibility += projected.z - shadow.parameters.y <= depth ? 1.0 : 0.0;
        }
    }
    return visibility / 9.0;
}

float shadowVisibility(GPULight light, vec3 normal, float viewZ) {
    uint base = light.metadata.y;
    uint count = light.metadata.z;
    if (base == 0xffffffffu || count == 0u || base >= uint(shadows.length())) {
        return 1.0;
    }
    count = min(count, uint(shadows.length()) - base);
    if (light.metadata.x == 0u) {
        for (uint cascade = 0u; cascade < count; ++cascade) {
            if (viewZ <= shadows[base + cascade].parameters.x) {
                return sampleShadow(base + cascade, normal);
            }
        }
        return 1.0;
    }
    uint face = 0u;
    if (light.metadata.x == 1u) {
        vec3 delta = v_world_position - light.positionRadius.xyz;
        vec3 magnitude = abs(delta);
        if (magnitude.x >= magnitude.y && magnitude.x >= magnitude.z) {
            face = delta.x >= 0.0 ? 0u : 1u;
        } else if (magnitude.y >= magnitude.z) {
            face = delta.y >= 0.0 ? 2u : 3u;
        } else {
            face = delta.z >= 0.0 ? 4u : 5u;
        }
    }
    return face < count ? sampleShadow(base + face, normal) : 1.0;
}

vec3 evaluateLight(uint index, vec3 normal, vec3 viewDirection, vec3 albedo,
                   float metallic, float roughness, float viewZ) {
    if (index >= min(uint(u_grid.w), uint(lights.length()))) {
        return vec3(0.0);
    }
    GPULight light = lights[index];
    vec3 direction;
    float attenuation = 1.0;
    if (light.metadata.x == 0u) {
        direction = safeNormalize(-light.directionOuter.xyz, normal);
    } else {
        vec3 delta = light.positionRadius.xyz - v_world_position;
        float distanceSquared = dot(delta, delta);
        float radius = light.positionRadius.w;
        if (radius <= 0.0 || distanceSquared >= radius * radius) {
            return vec3(0.0);
        }
        direction = safeNormalize(delta, normal);
        float ratioSquared = distanceSquared / (radius * radius);
        float window = max(1.0 - ratioSquared * ratioSquared, 0.0);
        attenuation = window * window / max(distanceSquared, 0.0001);
        if (light.metadata.x == 2u) {
            float cosine = dot(-direction, safeNormalize(light.directionOuter.xyz, vec3(1.0, 0.0, 0.0)));
            float outer = light.directionOuter.w;
            float inner = light.parameters.x;
            float cone = clamp((cosine - outer) / max(inner - outer, 0.00001), 0.0, 1.0);
            attenuation *= cone * cone * (3.0 - 2.0 * cone);
        }
    }
    float nDotL = max(dot(normal, direction), 0.0);
    float nDotV = max(dot(normal, viewDirection), 0.0001);
    if (nDotL <= 0.0 || attenuation <= 0.0) {
        return vec3(0.0);
    }
    vec3 halfway = safeNormalize(direction + viewDirection, normal);
    float nDotH = max(dot(normal, halfway), 0.0);
    float vDotH = max(dot(viewDirection, halfway), 0.0);
    float alpha = roughness * roughness;
    float alphaSquared = alpha * alpha;
    float denominator = nDotH * nDotH * (alphaSquared - 1.0) + 1.0;
    float distribution = alphaSquared / max(kPi * denominator * denominator, 1e-8);
    float smithV = 2.0 * nDotV / (nDotV + sqrt(alphaSquared + (1.0 - alphaSquared) * nDotV * nDotV));
    float smithL = 2.0 * nDotL / (nDotL + sqrt(alphaSquared + (1.0 - alphaSquared) * nDotL * nDotL));
    vec3 f0 = mix(vec3(0.04), albedo, metallic);
    vec3 fresnel = f0 + (1.0 - f0) * pow(1.0 - vDotH, 5.0);
    vec3 specular = distribution * smithV * smithL * fresnel / max(4.0 * nDotV * nDotL, 0.0001);
    vec3 diffuse = (1.0 - fresnel) * (1.0 - metallic) * albedo / kPi;
    vec3 radiance = light.colorIntensity.rgb * light.colorIntensity.w * attenuation;
    return (diffuse + specular) * radiance * nDotL * shadowVisibility(light, normal, viewZ);
}

void main() {
    vec4 base = texture(u_albedo_texture, v_uv) * u_base_color * v_color;
    vec4 metallicRoughness = texture(u_metallic_roughness_texture, v_uv);
    float metallic = clamp(u_material.x * metallicRoughness.b, 0.0, 1.0);
    float roughness = clamp(u_material.y * metallicRoughness.g, 0.045, 1.0);
    vec3 geometricNormal = safeNormalize(v_normal, vec3(0.0, 1.0, 0.0));
    vec3 tangentFallback = abs(geometricNormal.y) < 0.99
        ? normalize(cross(vec3(0.0, 1.0, 0.0), geometricNormal))
        : normalize(cross(vec3(1.0, 0.0, 0.0), geometricNormal));
    vec3 tangent = safeNormalize(v_tangent.xyz - geometricNormal * dot(geometricNormal, v_tangent.xyz), tangentFallback);
    vec3 bitangent = cross(geometricNormal, tangent) * (v_tangent.w < 0.0 ? -1.0 : 1.0);
    vec3 mapped = texture(u_normal_texture, v_uv).xyz * 2.0 - 1.0;
    mapped.xy *= u_material.z;
    vec3 normal = safeNormalize(mat3(tangent, bitangent, geometricNormal) * mapped, geometricNormal);
    vec3 viewDirection = safeNormalize(u_camera.xyz - v_world_position, normal);
    float viewZ = (u_view * vec4(v_world_position, 1.0)).z;
    uvec3 grid = max(uvec3(u_grid.xyz), uvec3(1u));
    uvec2 tile = uvec2(clamp(floor(gl_FragCoord.xy / u_screen.xy * vec2(grid.xy)), vec2(0.0), vec2(grid.xy - 1u)));
    float slice = floor(log(max(viewZ, u_z_parameters.x) / u_z_parameters.x) * u_z_parameters.z);
    uint z = uint(clamp(slice, 0.0, float(grid.z - 1u)));
    uint clusterIndex = (z * grid.y + tile.y) * grid.x + tile.x;
    vec3 color = vec3(0.0);
    if (clusterIndex < uint(clusters.length())) {
        uvec4 cluster = clusters[clusterIndex];
        if (cluster.z != 0u) {
            uint count = min(uint(u_grid.w), uint(lights.length()));
            for (uint i = 0u; i < count; ++i) {
                color += evaluateLight(i, normal, viewDirection, base.rgb, metallic, roughness, viewZ);
            }
        } else {
            uint available = uint(indices.length());
            uint count = cluster.x < available ? min(cluster.y, available - cluster.x) : 0u;
            for (uint i = 0u; i < count; ++i) {
                color += evaluateLight(indices[cluster.x + i], normal, viewDirection, base.rgb, metallic, roughness, viewZ);
            }
        }
    }
    float ao = clamp(texture(u_ao_texture, v_uv).r, 0.0, 1.0);
    vec3 ambientFresnel = mix(vec3(0.04), base.rgb, metallic);
    color += u_ambient.rgb * base.rgb * (1.0 - metallic) * (1.0 - ambientFresnel) * ao;
    color += texture(u_emissive_texture, v_uv).rgb * u_emission.rgb;
    frag_color = vec4(color, base.a);
}
