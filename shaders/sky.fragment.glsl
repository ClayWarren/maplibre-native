in vec2 v_pos;
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
