#pragma once

#include <mln/util/geo.hpp>
#include <mln/util/geometry.hpp>
#include <mln/util/mat4.hpp>

namespace mln {

class UnwrappedTileID;

struct ProjectionData {
    mat4 mainMatrix;
};

class ProjectionBase {
public:
    virtual ~ProjectionBase() = default;

    virtual Point<double> project(const LatLng&, double scale) const = 0;
    virtual LatLng unproject(const Point<double>&, double scale, LatLng::WrapMode) const = 0;

    virtual void tileMatrix(mat4&, const UnwrappedTileID&, double scale) const = 0;

    virtual ProjectionData getProjectionData(const UnwrappedTileID&, double scale, const mat4& projMatrix) const = 0;
};

} // namespace mln
