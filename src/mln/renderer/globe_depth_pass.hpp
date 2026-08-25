#pragma once

#include <mln/renderer/layer_group.hpp>
#include <mln/shaders/shader_program_base.hpp>

namespace mln {

namespace gfx {
class Context;
class ShaderRegistry;
} // namespace gfx

class TransformState;
class UpdateParameters;

/// Writes the planet into the depth buffer ahead of the translucent pass, so that 3D geometry
/// behind the horizon is hidden by the globe the way the 2D layers are by their clip Z.
class GlobeDepthPass {
public:
    void update(gfx::ShaderRegistry&, gfx::Context&, const TransformState&, const UpdateParameters&);

    LayerGroupBase* getLayerGroup() const { return layerGroup.get(); }

private:
    TileLayerGroupPtr layerGroup;
    gfx::ShaderProgramBasePtr shader;
};

} // namespace mln
