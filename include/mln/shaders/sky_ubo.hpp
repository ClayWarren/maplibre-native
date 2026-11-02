#pragma once

#include <mln/util/color.hpp>

#include <array>

namespace mln {
namespace shaders {

struct alignas(16) SkyPropsUBO {
    /*  0 */ Color sky_color;
    /* 16 */ Color horizon_color;
    /* 32 */ std::array<float, 2> horizon;
    /* 40 */ std::array<float, 2> horizon_normal;
    /* 48 */ std::array<float, 2> viewport_size;
    /* 56 */ float sky_horizon_blend;
    /* 60 */ float sky_blend;
    /* 64 */ Color backdrop_color;
    /* 80 */ std::array<float, 4 * 4> star_matrix;
    /* 144 */ std::array<float, 4 * 4> inv_view_projection;
    /* 208 */ std::array<float, 4> camera_position;
    /* 224 */ float star_opacity;
    /* 228 */ float pixel_ratio;
    /* 232 */ std::array<float, 2> padding{};
};
static_assert(sizeof(SkyPropsUBO) == 15 * 16);

struct alignas(16) AtmospherePropsUBO {
    /*  0 */ std::array<float, 4 * 4> inv_view_projection;
    /* 64 */ std::array<float, 4> camera_position;
    // The fourth component stores the evaluated atmosphere blend.
    /* 80 */ std::array<float, 4> sun_position;
    /* 96 */
};
static_assert(sizeof(AtmospherePropsUBO) == 6 * 16);

} // namespace shaders
} // namespace mln
