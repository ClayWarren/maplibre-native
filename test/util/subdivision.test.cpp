#include <mln/test/util.hpp>

#include <mln/util/constants.hpp>
#include <mln/util/subdivision.hpp>
#include <mln/util/subdivision_granularity.hpp>
#include <mln/util/tile_mesh.hpp>

#include <map>
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

namespace {

// Every interior edge is shared by exactly two triangles and every boundary edge by one: no cracks, no overlaps.
void expectManifold(const SubdivisionResult& r) {
    std::map<std::pair<uint32_t, uint32_t>, int> edges;
    for (std::size_t i = 0; i + 2 < r.triangleIndices.size(); i += 3) {
        for (std::size_t k = 0; k < 3; k++) {
            const uint32_t a = r.triangleIndices[i + k];
            const uint32_t b = r.triangleIndices[i + (k + 1) % 3];
            edges[{std::min(a, b), std::max(a, b)}]++;
        }
    }
    // Pole quads are generated once per adjacent triangle, so edges at the poles are legitimately shared more.
    const auto touchesPole = [&](const std::pair<uint32_t, uint32_t>& edge) {
        const int16_t y0 = r.vertices[edge.first * 2 + 1];
        const int16_t y1 = r.vertices[edge.second * 2 + 1];
        const auto atPole = [](int16_t y) {
            return y == NORTH_POLE_Y || y == SOUTH_POLE_Y;
        };
        const auto onPoleEdge = [](int16_t y) {
            return y == 0 || y == EXTENT;
        };
        return atPole(y0) || atPole(y1) || (onPoleEdge(y0) && y0 == y1);
    };
    std::size_t bad = 0;
    for (const auto& [edge, count] : edges) {
        if (count > 2 && !touchesPole(edge)) {
            bad++;
        }
    }
    EXPECT_EQ(0u, bad);
    // T-junctions show up as boundary edges that are not on the outline: a vertex lying strictly inside another edge.
    std::size_t tJunctions = 0;
    for (const auto& [edge, count] : edges) {
        if (count != 1) continue;
        const double ax = r.vertices[edge.first * 2], ay = r.vertices[edge.first * 2 + 1];
        const double bx = r.vertices[edge.second * 2], by = r.vertices[edge.second * 2 + 1];
        for (std::size_t v = 0; v < r.vertices.size() / 2; v++) {
            if (v == edge.first || v == edge.second) continue;
            const double px = r.vertices[v * 2], py = r.vertices[v * 2 + 1];
            const double cross = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
            if (cross != 0) continue;
            const double dot = (px - ax) * (bx - ax) + (py - ay) * (by - ay);
            const double len2 = (bx - ax) * (bx - ax) + (by - ay) * (by - ay);
            if (dot > 0 && dot < len2) {
                tJunctions++;
                break;
            }
        }
    }
    EXPECT_EQ(0u, tJunctions);
}

} // namespace

TEST(Subdivision, WorldPolygonWithBufferIsManifold) {
    constexpr int16_t buffer = 512;
    const auto world = square(-buffer, -buffer, EXTENT + buffer, EXTENT + buffer);
    for (const uint8_t z : {0, 1}) {
        const auto result = subdividePolygon(world, CanonicalTileID(z, 0, 0), 128u >> z);
        expectManifold(result);
    }
}

TEST(TileMesh, QuadAndGrid) {
    const auto quad = createTileMesh({.granularity = 1});
    EXPECT_EQ(4u, quad.vertices.size() / 2);
    EXPECT_EQ(6u, quad.indices.size());

    const auto grid = createTileMesh({.granularity = 4, .generateBorders = true});
    EXPECT_EQ(7u * 7u, grid.vertices.size() / 2);
    EXPECT_EQ(6u * 6u * 6u, grid.indices.size());
    EXPECT_EQ(-EXTENT / 128, grid.vertices[0]);
    EXPECT_EQ(EXTENT + EXTENT / 128, grid.vertices[(7 * 7 - 1) * 2]);

    const auto polar = createTileMesh({.granularity = 2, .extendToNorthPole = true, .extendToSouthPole = true});
    EXPECT_EQ(3u * 5u, polar.vertices.size() / 2);
    EXPECT_EQ(NORTH_POLE_Y, polar.vertices[1]);
    EXPECT_EQ(SOUTH_POLE_Y, polar.vertices[(3 * 5 - 1) * 2 + 1]);
    for (std::size_t i = 0; i + 2 < polar.indices.size(); i += 3) {
        const auto x = [&](uint16_t v) {
            return static_cast<double>(polar.vertices[v * 2]);
        };
        const auto y = [&](uint16_t v) {
            return static_cast<double>(polar.vertices[v * 2 + 1]);
        };
        const double cross = (x(polar.indices[i + 1]) - x(polar.indices[i])) *
                                 (y(polar.indices[i + 2]) - y(polar.indices[i])) -
                             (y(polar.indices[i + 1]) - y(polar.indices[i])) *
                                 (x(polar.indices[i + 2]) - x(polar.indices[i]));
        EXPECT_LT(cross, 0.0);
    }
}
