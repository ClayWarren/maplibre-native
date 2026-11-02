layout(location = 0) in vec2 a_pos;
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
