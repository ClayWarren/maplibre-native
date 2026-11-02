#include <mln/test/util.hpp>
#include <mln/gfx/headless_frontend.hpp>
#include <mln/map/map.hpp>
#include <mln/map/map_options.hpp>
#include <mln/storage/resource_options.hpp>
#include <mln/style/style.hpp>
#include <mln/style/sky.hpp>
#include <mln/util/run_loop.hpp>
#include <cstring>

using namespace mln;

namespace {
std::size_t brightPixels(const PremultipliedImage& image) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < image.bytes(); i += 4) {
        if (image.data[i] > 20) ++count;
    }
    return count;
}
} // namespace

TEST(SkyRendering, StarsRotateDisappearAndReturn) {
    util::RunLoop loop;
    HeadlessFrontend frontend{1};
    Map map(frontend,
            MapObserver::nullObserver(),
            MapOptions().withMapMode(MapMode::Static).withSize(frontend.getSize()),
            ResourceOptions().withCachePath(":memory:"));
    map.getStyle().loadJSON(R"({"version":8,"projection":{"type":"globe"},
        "sky":{"star-opacity":1,"backdrop-color":"black","atmosphere-blend":0},
        "sources":{},"layers":[]})");
    map.jumpTo(CameraOptions().withCenter(LatLng{0, 0}).withZoom(0));
    const auto first = frontend.render(map).image;
    EXPECT_GT(brightPixels(first), 10u);
    // No opaque layers: the renderer itself must hide stars through the globe.
    const auto cx = first.size.width / 2;
    const auto cy = first.size.height / 2;
    for (uint32_t y = cy - 20; y < cy + 20; ++y) {
        for (uint32_t x = cx - 20; x < cx + 20; ++x) {
            EXPECT_EQ(0, first.data[(y * first.size.width + x) * 4]);
        }
    }
    map.jumpTo(CameraOptions().withCenter(LatLng{20, 70}).withBearing(30));
    const auto rotated = frontend.render(map).image;
    EXPECT_GT(brightPixels(rotated), 10u);
    EXPECT_NE(0, std::memcmp(first.data.get(), rotated.data.get(), first.bytes()));
    map.getStyle().getSky()->setStarOpacity(0.0f);
    EXPECT_EQ(0u, brightPixels(frontend.render(map).image));
    map.jumpTo(CameraOptions().withCenter(LatLng{0, 0}).withBearing(0));
    map.getStyle().getSky()->setStarOpacity(1.0f);
    const auto restored = frontend.render(map).image;
    EXPECT_EQ(0, std::memcmp(first.data.get(), restored.data.get(), first.bytes()));
    map.getStyle().setSky(nullptr);
    EXPECT_EQ(0u, brightPixels(frontend.render(map).image));
}

TEST(SkyRendering, BackdropAndOpaquePlanarSky) {
    util::RunLoop loop;
    HeadlessFrontend frontend{1};
    Map map(frontend,
            MapObserver::nullObserver(),
            MapOptions().withMapMode(MapMode::Static).withSize(frontend.getSize()),
            ResourceOptions().withCachePath(":memory:"));
    map.getStyle().loadJSON(R"({"version":8,
        "sky":{"star-opacity":1,"backdrop-color":"black","sky-color":"black","horizon-color":"black"},
        "sources":{},"layers":[]})");
    map.jumpTo(CameraOptions().withZoom(0).withPitch(70));
    const auto image = frontend.render(map).image;
    EXPECT_EQ(0u, brightPixels(image));
    for (std::size_t i = 3; i < image.bytes(); i += 4) EXPECT_EQ(255, image.data[i]);
}
