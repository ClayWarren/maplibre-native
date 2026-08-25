#include <mln/shaders/vulkan/globe_depth.hpp>
#include <mln/shaders/shader_defines.hpp>

namespace mln {
namespace shaders {

using GlobeDepthShaderSource = ShaderSource<BuiltIn::GlobeDepthShader, gfx::Backend::Type::Vulkan>;

const std::array<AttributeInfo, 1> GlobeDepthShaderSource::attributes = {
    AttributeInfo{0, gfx::AttributeDataType::Float3, idGlobeDepthPosVertexAttribute},
};
const std::array<TextureInfo, 0> GlobeDepthShaderSource::textures = {};

} // namespace shaders
} // namespace mln
