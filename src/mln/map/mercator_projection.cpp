#include <mln/map/mercator_projection.hpp>
#include <mln/tile/tile_id.hpp>
#include <mln/util/constants.hpp>
#include <mln/util/projection.hpp>

namespace mln {

Point<double> MercatorProjection::project(const LatLng& latLng, double scale) const {
    return Projection::project(latLng, scale);
}

LatLng MercatorProjection::unproject(const Point<double>& point, double scale, LatLng::WrapMode wrapMode) const {
    return Projection::unproject(point, scale, wrapMode);
}

void MercatorProjection::tileMatrix(mat4& matrix, const UnwrappedTileID& tileID, double scale) const {
    const uint64_t tileScale = 1ull << tileID.canonical.z;
    const double s = Projection::worldSize(scale) / tileScale;

    matrix::identity(matrix);
    matrix::translate(matrix,
                      matrix,
                      int64_t(tileID.canonical.x + tileID.wrap * static_cast<int64_t>(tileScale)) * s,
                      int64_t(tileID.canonical.y) * s,
                      0);
    matrix::scale(matrix, matrix, s / util::EXTENT, s / util::EXTENT, 1);
}

ProjectionData MercatorProjection::getProjectionData(const UnwrappedTileID& tileID,
                                                     double scale,
                                                     const mat4& projMatrix) const {
    ProjectionData data;
    tileMatrix(data.mainMatrix, tileID, scale);
    matrix::multiply(data.mainMatrix, projMatrix, data.mainMatrix);
    return data;
}

} // namespace mln
