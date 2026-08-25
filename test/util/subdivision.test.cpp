#include <mln/test/util.hpp>

#include <mln/util/constants.hpp>
#include <mln/util/subdivision.hpp>
#include <mln/util/subdivision_granularity.hpp>

#include <set>

using namespace mln;
using namespace mln::util;

namespace {

GeometryCollection square(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    return {{{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}, {x0, y0}}};
}

double signedArea(const std::vector<int16_t>& v, uint32_t i0, uint32_t i1, uint32_t i2) {
    const double e0x = v[i1 * 2] - v[i0 * 2];
    const double e0y = v[i1 * 2 + 1] - v[i0 * 2 + 1];
    const double e1x = v[i2 * 2] - v[i0 * 2];
    const double e1y = v[i2 * 2 + 1] - v[i0 * 2 + 1];
    return e0x * e1y - e0y * e1x;
}

double totalArea(const SubdivisionResult& r) {
    double area = 0;
    for (std::size_t i = 0; i + 2 < r.triangleIndices.size(); i += 3) {
        area += std::abs(
                    signedArea(r.vertices, r.triangleIndices[i], r.triangleIndices[i + 1], r.triangleIndices[i + 2])) /
                2;
    }
    return area;
}

} // namespace

TEST(Subdivision, Granularity) {
    const SubdivisionGranularityExpression fill{128, 2};
    EXPECT_EQ(128u, fill.getGranularityForZoomLevel(0));
    EXPECT_EQ(64u, fill.getGranularityForZoomLevel(1));
    EXPECT_EQ(2u, fill.getGranularityForZoomLevel(6));
    EXPECT_EQ(2u, fill.getGranularityForZoomLevel(12));
    EXPECT_EQ(1u, SubdivisionGranularityExpression(0, 0).getGranularityForZoomLevel(3));
    EXPECT_EQ(SubdivisionGranularitySetting::none(), SubdivisionGranularitySetting{});
}

TEST(Subdivision, NoSubdivisionKeepsTriangles) {
    const auto result = subdividePolygon(square(0, 0, 100, 100), CanonicalTileID(3, 1, 1), 1);
    EXPECT_EQ(4u, result.vertices.size() / 2);
    EXPECT_EQ(6u, result.triangleIndices.size());
    EXPECT_DOUBLE_EQ(100.0 * 100.0, totalArea(result));
}

TEST(Subdivision, SplitsOnCellBoundariesAndKeepsArea) {
    const auto full = square(0, 0, EXTENT, EXTENT);
    const auto result = subdividePolygon(full, CanonicalTileID(3, 1, 1), 4);
    // A 4x4 grid: 25 unique vertices, 32 triangles.
    EXPECT_EQ(25u, result.vertices.size() / 2);
    EXPECT_EQ(32u * 3u, result.triangleIndices.size());
    EXPECT_DOUBLE_EQ(static_cast<double>(EXTENT) * EXTENT, totalArea(result));
    for (std::size_t i = 0; i + 2 < result.triangleIndices.size(); i += 3) {
        EXPECT_LT(signedArea(result.vertices,
                             result.triangleIndices[i],
                             result.triangleIndices[i + 1],
                             result.triangleIndices[i + 2]),
                  0.0);
    }
    ASSERT_EQ(1u, result.lineIndexLists.size());
    EXPECT_EQ(16u * 2u, result.lineIndexLists[0].size());
}

TEST(Subdivision, PoleTilesGetPoleQuads) {
    const auto full = square(0, 0, EXTENT, EXTENT);
    const auto north = subdividePolygon(full, CanonicalTileID(1, 0, 0), 2);
    std::set<int16_t> ys;
    for (std::size_t i = 1; i < north.vertices.size(); i += 2) {
        ys.insert(north.vertices[i]);
    }
    EXPECT_TRUE(ys.contains(NORTH_POLE_Y));
    EXPECT_FALSE(ys.contains(SOUTH_POLE_Y));

    const auto south = subdividePolygon(full, CanonicalTileID(1, 0, 1), 2);
    ys.clear();
    for (std::size_t i = 1; i < south.vertices.size(); i += 2) {
        ys.insert(south.vertices[i]);
    }
    EXPECT_TRUE(ys.contains(SOUTH_POLE_Y));
    EXPECT_FALSE(ys.contains(NORTH_POLE_Y));

    const auto middle = subdividePolygon(full, CanonicalTileID(2, 1, 1), 2);
    for (std::size_t i = 1; i < middle.vertices.size(); i += 2) {
        EXPECT_NE(NORTH_POLE_Y, middle.vertices[i]);
        EXPECT_NE(SOUTH_POLE_Y, middle.vertices[i]);
    }
}

TEST(Subdivision, ZoomZeroDropsGeometryOutsideTheTile) {
    const auto wide = square(-2000, 1000, EXTENT + 2000, 2000);
    const auto result = subdividePolygon(wide, CanonicalTileID(0, 0, 0), 8);
    for (std::size_t i = 0; i + 2 < result.triangleIndices.size(); i += 3) {
        bool inside = false;
        for (std::size_t k = 0; k < 3; k++) {
            const int16_t x = result.vertices[result.triangleIndices[i + k] * 2];
            inside |= x >= 0 && x <= EXTENT;
        }
        EXPECT_TRUE(inside);
    }
}

TEST(Subdivision, VertexLine) {
    const GeometryCoordinates line{{0, 0}, {EXTENT, 0}};
    EXPECT_EQ(line, subdivideVertexLine(line, 1));
    const auto split = subdivideVertexLine(line, 4);
    ASSERT_EQ(5u, split.size());
    EXPECT_EQ(GeometryCoordinate(EXTENT / 4, 0), split[1]);
    EXPECT_EQ(GeometryCoordinate(EXTENT, 0), split.back());

    const GeometryCoordinates diagonal{{0, 0}, {EXTENT, EXTENT}};
    const auto diagonalSplit = subdivideVertexLine(diagonal, 2);
    ASSERT_EQ(3u, diagonalSplit.size());
    EXPECT_EQ(GeometryCoordinate(EXTENT / 2, EXTENT / 2), diagonalSplit[1]);

    const GeometryCoordinates ring{{0, 0}, {EXTENT, 0}, {EXTENT, EXTENT}};
    const auto closed = subdivideVertexLine(ring, 1, true);
    EXPECT_EQ(ring.front(), closed.back());
    EXPECT_EQ(4u, closed.size());
}
