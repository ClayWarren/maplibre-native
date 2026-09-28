#include <mln/renderer/sky_pass.hpp>

#include <mln/gfx/color_mode.hpp>
#include <mln/gfx/context.hpp>
#include <mln/gfx/cull_face_mode.hpp>
#include <mln/gfx/drawable_builder.hpp>
#include <mln/gfx/projection_variant.hpp>
#include <mln/gfx/shader_registry.hpp>
#include <mln/map/transform_state.hpp>
#include <mln/map/vertical_perspective_projection.hpp>
#include <mln/renderer/render_pass.hpp>
#include <mln/shaders/shader_defines.hpp>
#include <mln/shaders/sky_ubo.hpp>
#include <mln/style/light_impl.hpp>
#include <mln/style/sky_impl.hpp>
#include <mln/util/convert.hpp>
#include <mln/util/mat4.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string_view>

namespace mln {

namespace {

constexpr std::string_view SkyShaderName = "SkyShader";
constexpr std::string_view AtmosphereShaderName = "AtmosphereShader";

// Same Fibonacci sphere and magnitude sequence as our GL JS fork. Encode the star
// index and billboard corner in Short2 vertices; rotation stays on the GPU.
void addStars(LayerGroup& group, gfx::Context& context, const gfx::ShaderProgramBasePtr& shader) {
    auto builder = context.createDrawableBuilder("stars");
    builder->setShader(shader);
    builder->setRenderPass(RenderPass::Translucent);
    builder->setEnableDepth(false);
    builder->setEnableStencil(false);
    builder->setColorMode(gfx::ColorMode::alphaBlended());
    builder->setCullFaceMode(gfx::CullFaceMode::disabled());
    builder->setVertexAttrId(shaders::idSkyPosVertexAttribute);
    for (int16_t star = 2; star < 2050; ++star) {
        builder->addTriangle(star, 0, star, 1, star, 2);
        builder->addTriangle(star, 0, star, 2, star, 3);
    }
    builder->flush(context);
    for (auto& drawable : builder->clearDrawables()) {
        group.addDrawable(std::move(drawable));
    }
}

LayerGroupPtr makeFullscreenLayerGroup(gfx::Context& context,
                                       const gfx::ShaderProgramBasePtr& shader,
                                       std::string_view name,
                                       std::size_t vertexAttribute,
                                       bool is3D,
                                       gfx::DepthMaskType depthMask) {
    if (!shader) {
        return nullptr;
    }

    auto layerGroup = context.createLayerGroup(0, 1, std::string(name));
    if (!layerGroup) {
        return nullptr;
    }

    auto builder = context.createDrawableBuilder(std::string(name));
    builder->setShader(shader);
    builder->setRenderPass(RenderPass::Translucent);
    builder->setIs3D(is3D);
    builder->setDepthType(depthMask);
    builder->setEnableDepth(true);
    builder->setEnableStencil(false);
    builder->setColorMode(gfx::ColorMode::alphaBlended());
    builder->setCullFaceMode(gfx::CullFaceMode::disabled());
    builder->setVertexAttrId(vertexAttribute);
    builder->addQuad(-1, -1, 1, 1);
    builder->flush(context);

    for (auto& drawable : builder->clearDrawables()) {
        layerGroup->addDrawable(std::move(drawable));
    }
    return layerGroup;
}

std::array<float, 4> sunPosition(const EvaluatedLight& light, const TransformState& state, float blend) {
    const auto position = light.get<style::LightPosition>().getCartesian();
    vec4 sun = {{-position[0], -position[1], -position[2], 0.0}};

    if (light.get<style::LightAnchor>() == style::LightAnchorType::Map) {
        mat4 matrix;
        matrix::identity(matrix);
        matrix::rotate_z(matrix, matrix, state.getRoll());
        matrix::rotate_x(matrix, matrix, -state.getPitch());
        matrix::rotate_z(matrix, matrix, -state.getBearing());
        matrix::rotate_x(matrix, matrix, state.getLatLng().latitude() * std::numbers::pi / 180.0);
        matrix::rotate_y(matrix, matrix, -state.getLatLng().longitude() * std::numbers::pi / 180.0);
        matrix::transformMat4(sun, sun, matrix);
    }

    return {{static_cast<float>(sun[0]), static_cast<float>(sun[1]), static_cast<float>(sun[2]), blend}};
}

} // namespace

void SkyPass::update(gfx::ShaderRegistry& shaders,
                     gfx::Context& context,
                     const TransformState& state,
                     const Size& viewportSize,
                     float pixelRatio,
                     const std::optional<EvaluatedSky>& evaluated,
                     const EvaluatedLight& light) {
    if (!evaluated || viewportSize.isEmpty()) {
        if (skyLayerGroup) {
            skyLayerGroup->clearDrawables();
        }
        if (atmosphereLayerGroup) {
            atmosphereLayerGroup->clearDrawables();
        }
        return;
    }

    const double projectionTransition = std::clamp(state.getProjectionTransition(), 0.0, 1.0);
    const float starOpacity = std::clamp(evaluated->get<style::SkyStarOpacity>(), 0.0f, 1.0f);
    const bool wantsStars = starOpacity > 0.0f;
    if (projectionTransition < 1.0 || wantsStars || evaluated->get<style::SkyBackdropColor>().a > 0.0f) {
        if (!skyShader) {
            skyShader = context.getGenericShader(shaders, std::string(SkyShaderName), gfx::ProjectionVariant::Mercator);
        }
        if (!skyLayerGroup || skyLayerGroup->empty() || wantsStars != hasStars) {
            skyLayerGroup = makeFullscreenLayerGroup(
                context, skyShader, "sky", shaders::idSkyPosVertexAttribute, false, gfx::DepthMaskType::ReadWrite);
            hasStars = wantsStars;
            if (skyLayerGroup && wantsStars) {
                addStars(*skyLayerGroup, context, skyShader);
            }
        }

        if (skyLayerGroup) {
            constexpr double maxHorizonAngle = 89.25 * std::numbers::pi / 180.0;
            const double pitch = state.getPitch();
            const double horizonDistance = state.getCameraToCenterDistance() * pixelRatio *
                                           std::min(std::tan(std::numbers::pi / 2.0 - pitch) * 0.85,
                                                    std::tan(maxHorizonAngle - pitch));
            const double roll = state.getRoll();
            const std::array<float, 2> normal = {
                {static_cast<float>(-std::sin(roll)), static_cast<float>(std::cos(roll))}};
            const std::array<float, 2> horizon = {
                {static_cast<float>(viewportSize.width * 0.5 + normal[0] * horizonDistance),
                 static_cast<float>(viewportSize.height * 0.5 + normal[1] * horizonDistance)}};

            mat4 inverse;
            matrix::identity(inverse);
            vec3 camera{};
            if (state.isGlobeRendering()) {
                inverse = state.getInverseGlobeViewProjectionMatrix();
                camera = state.getGlobeCameraPosition();
            }
            const shaders::SkyPropsUBO props = {
                .sky_color = evaluated->get<style::SkyColor>(),
                .horizon_color = evaluated->get<style::SkyHorizonColor>(),
                .horizon = horizon,
                .horizon_normal = normal,
                .viewport_size = {{static_cast<float>(viewportSize.width), static_cast<float>(viewportSize.height)}},
                .sky_horizon_blend = evaluated->get<style::SkyHorizonBlend>() * viewportSize.height * 0.5f,
                .sky_blend = static_cast<float>(projectionTransition),
                .backdrop_color = evaluated->get<style::SkyBackdropColor>(),
                // w=0 discards all translation, so zoom never moves the stars.
                .star_matrix = util::cast<float>(VerticalPerspectiveProjection::globeViewProjectionMatrix(state, 1.0)),
                .inv_view_projection = util::cast<float>(inverse),
                .camera_position = {{static_cast<float>(camera[0]),
                                     static_cast<float>(camera[1]),
                                     static_cast<float>(camera[2]),
                                     0.0f}},
                .star_opacity = starOpacity,
                .pixel_ratio = pixelRatio,
            };
            skyLayerGroup->mutableUniformBuffers().createOrUpdate(shaders::idSkyPropsUBO, &props, context);
        }
    } else if (skyLayerGroup) {
        skyLayerGroup->clearDrawables();
    }

    const float atmosphereBlend = evaluated->get<style::SkyAtmosphereBlend>() *
                                  static_cast<float>(projectionTransition);
    if (!state.isGlobeRendering() || atmosphereBlend <= 0.0f) {
        if (atmosphereLayerGroup) {
            atmosphereLayerGroup->clearDrawables();
        }
        return;
    }

    if (!atmosphereShader) {
        atmosphereShader = context.getGenericShader(
            shaders, std::string(AtmosphereShaderName), gfx::ProjectionVariant::Globe);
    }
    if (!atmosphereLayerGroup || atmosphereLayerGroup->empty()) {
        atmosphereLayerGroup = makeFullscreenLayerGroup(context,
                                                        atmosphereShader,
                                                        "atmosphere",
                                                        shaders::idAtmospherePosVertexAttribute,
                                                        true,
                                                        gfx::DepthMaskType::ReadOnly);
    }
    if (!atmosphereLayerGroup) {
        return;
    }

    const auto& camera = state.getGlobeCameraPosition();
    const shaders::AtmospherePropsUBO props = {
        .inv_view_projection = util::cast<float>(state.getInverseGlobeViewProjectionMatrix()),
        .camera_position =
            {{static_cast<float>(camera[0]), static_cast<float>(camera[1]), static_cast<float>(camera[2]), 0.0f}},
        .sun_position = sunPosition(light, state, atmosphereBlend),
    };
    atmosphereLayerGroup->mutableUniformBuffers().createOrUpdate(shaders::idAtmospherePropsUBO, &props, context);
}

} // namespace mln
