// Generated code, do not modify this file!
#pragma once
#include <mln/shaders/shader_source.hpp>

namespace mln {
namespace shaders {

template <>
struct ShaderSource<BuiltIn::SkyShader, gfx::Backend::Type::OpenGL> {
    static constexpr const char* name = "SkyShader";
    static constexpr const char* vertex = R"(layout(location = 0) in vec2 a_pos;
out vec2 v_pos;
out vec3 v_star;

layout(std140) uniform SkyPropsUBO {
    vec4 sky_color;
    vec4 horizon_color;
    vec2 horizon;
    vec2 horizon_normal;
    vec2 viewport_size;
    float sky_horizon_blend;
    float sky_blend;
    vec4 backdrop_color;
    mat4 star_matrix;
    mat4 inv_view_projection;
    vec4 camera_position;
    float star_opacity;
    float pixel_ratio;
    vec2 padding;
} sky;

void main() {
    v_pos = a_pos;
    v_star = vec3(0.0);
    gl_Position = vec4(a_pos, 1.0, 1.0);
    if (a_pos.x < -1.0) {
        v_pos.x += 3.0;
        gl_Position.x += 3.0;
        v_star.z = -1.0;
    }
    if (a_pos.x >= 2.0) {
        float index = a_pos.x - 2.0;
        float height = 1.0 - 2.0 * (index + 0.5) / 2048.0;
        float longitude = index * 2.399963229728653;
        float radius = sqrt(max(0.0, 1.0 - height * height));
        vec3 direction = vec3(sin(longitude) * radius, height, cos(longitude) * radius);
        vec2 corner = vec2(a_pos.y == 1.0 || a_pos.y == 2.0 ? 1.0 : -1.0,
                           a_pos.y >= 2.0 ? 1.0 : -1.0);
        float magnitude = mod(index * 73.0, 101.0) / 100.0;
        float size = 0.75 + magnitude * magnitude;
        vec4 projected = sky.star_matrix * vec4(direction, 0.0);
        projected.xy += corner * size * 2.0 * sky.pixel_ratio / sky.viewport_size * projected.w;
        gl_Position = vec4(projected.xy, projected.w, projected.w);
        v_pos = projected.xy / projected.w;
        v_star = vec3(corner, 0.25 + 0.75 * magnitude);
    }
}
)";
    static constexpr const char* fragment = R"(in vec2 v_pos;
in vec3 v_star;

layout(std140) uniform SkyPropsUBO {
    vec4 sky_color;
    vec4 horizon_color;
    vec2 horizon;
    vec2 horizon_normal;
    vec2 viewport_size;
    float sky_horizon_blend;
    float sky_blend;
    vec4 backdrop_color;
    mat4 star_matrix;
    mat4 inv_view_projection;
    vec4 camera_position;
    float star_opacity;
    float pixel_ratio;
    vec2 padding;
} sky;

void main() {
    vec2 pixel = (v_pos * 0.5 + 0.5) * sky.viewport_size;
    float distance_to_horizon = dot(pixel - sky.horizon, sky.horizon_normal);
    vec4 color = vec4(0.0);
    if (distance_to_horizon > 0.0) {
        if (sky.sky_horizon_blend > 0.0 && distance_to_horizon < sky.sky_horizon_blend) {
            float blend = 1.0 - distance_to_horizon / sky.sky_horizon_blend;
            color = mix(sky.sky_color, sky.horizon_color, blend * blend);
        } else {
            color = sky.sky_color;
        }
    }
    color *= 1.0 - sky.sky_blend;
    if (v_star.z > 0.0) {
        float outside_globe = 1.0;
        if (sky.sky_blend > 0.0) {
            vec4 target = sky.inv_view_projection * vec4(v_pos, 0.0, 1.0);
            vec3 ray = normalize(target.xyz / target.w - sky.camera_position.xyz);
            float closest = dot(ray, -sky.camera_position.xyz);
            float distance_squared = dot(sky.camera_position.xyz, sky.camera_position.xyz) - closest * closest;
            outside_globe = closest <= 0.0 || distance_squared > 1.0 ? 1.0 : 0.0;
        }
        float visibility = mix(step(0.0, distance_to_horizon), outside_globe, sky.sky_blend);
        float alpha = (1.0 - smoothstep(0.1, 1.0, length(v_star.xy))) *
                      v_star.z * sky.star_opacity * visibility;
        fragColor = vec4(vec3(alpha), alpha);
    } else {
        fragColor = v_star.z < 0.0 ? color : (sky.star_opacity > 0.0 ? sky.backdrop_color :
                    color + sky.backdrop_color * (1.0 - color.a));
    }
}
)";
};

} // namespace shaders
} // namespace mln
