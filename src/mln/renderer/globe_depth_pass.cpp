#include <mln/renderer/globe_depth_pass.hpp>

#include <mln/gfx/context.hpp>
#include <mln/gfx/cull_face_mode.hpp>
#include <mln/gfx/drawable.hpp>
#include <mln/gfx/drawable_builder.hpp>
#include <mln/gfx/drawable_tweaker.hpp>
#include <mln/gfx/projection_variant.hpp>
#include <mln/gfx/shader_registry.hpp>
#include <mln/gfx/uniform_buffer.hpp>
#include <mln/map/transform_state.hpp>
#include <mln/renderer/globe_tile_mesh.hpp>
#include <mln/renderer/layer_tweaker.hpp>
#include <mln/renderer/paint_parameters.hpp>
#include <mln/renderer/render_pass.hpp>
#include <mln/renderer/update_parameters.hpp>
#include <mln/shaders/shader_defines.hpp>
#include <mln/util/tile_cover.hpp>

#include <algorithm>

namespace mln {

namespace {

constexpr std::string_view GlobeDepthShaderName = "GlobeDepthShader";

class GlobeDepthTweaker : public gfx::DrawableTweaker {
public:
    void init(gfx::Drawable&) override {}

    void execute(gfx::Drawable& drawable, PaintParameters& parameters) override {
        if (!drawable.getTileID()) {
            return;
        }
        const auto ubo = LayerTweaker::toProjectionUBO(
            parameters.projectionDataForTile(drawable.getTileID()->toUnwrapped()));
        drawable.mutableUniformBuffers().createOrUpdate(shaders::idProjectionUBO, &ubo, parameters.context);
    }
};

} // namespace

void GlobeDepthPass::update(gfx::ShaderRegistry& shaders,
                            gfx::Context& context,
                            const TransformState& state,
                            const UpdateParameters& updateParameters) {
    if (!state.isGlobeRendering()) {
        if (layerGroup) {
            layerGroup->clearDrawables();
        }
        return;
    }

    if (!shader) {
        shader = context.getGenericShader(shaders, std::string(GlobeDepthShaderName), gfx::ProjectionVariant::Globe);
    }
    if (!shader) {
        return;
    }
    if (!layerGroup) {
        layerGroup = context.createTileLayerGroup(0, /*initialCapacity=*/64, "globe-depth");
    }

    const auto zoom = state.getIntegerZoom();
    const auto tileCover = util::tileCover({.transformState = state,
                                            .tileLodMinRadius = updateParameters.tileLodMinRadius,
                                            .tileLodScale = updateParameters.tileLodScale,
                                            .tileLodPitchThreshold = updateParameters.tileLodPitchThreshold,
                                            .tileLodMode = updateParameters.tileLodMode},
                                           zoom,
                                           Range<uint8_t>(0, zoom));

    layerGroup->removeDrawablesIf([&](gfx::Drawable& drawable) {
        return drawable.getTileID() && (std::ranges::find(tileCover, *drawable.getTileID()) == tileCover.end());
    });

    std::unique_ptr<gfx::DrawableBuilder> builder;
    for (const auto& tileID : tileCover) {
        if (layerGroup->getDrawableCount(RenderPass::Translucent, tileID) > 0) {
            continue;
        }
        if (!builder) {
            builder = context.createDrawableBuilder("globe-depth");
            builder->setShader(shader);
            builder->setRenderPass(RenderPass::Translucent);
            builder->setIs3D(true);
            builder->setEnableColor(false);
            builder->setEnableDepth(true);
            builder->setDepthType(gfx::DepthMaskType::ReadWrite);
            builder->setCullFaceMode(gfx::CullFaceMode::backCCW());
            builder->setVertexAttrId(shaders::idGlobeDepthPosVertexAttribute);
            builder->addTweaker(std::make_shared<GlobeDepthTweaker>());
        }

        auto mesh = rawGlobeTileMesh(tileID.canonical, true);
        builder->setRawVertices(std::move(mesh.vertices), mesh.vertexCount, gfx::AttributeDataType::Short2);
        builder->setSegments(gfx::Triangles(), std::move(mesh.indices), mesh.segments.data(), mesh.segments.size());
        builder->flush(context);

        for (auto& drawable : builder->clearDrawables()) {
            drawable->setTileID(tileID);
            layerGroup->addDrawable(RenderPass::Translucent, tileID, std::move(drawable));
        }
    }
}

} // namespace mln
