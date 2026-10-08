/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <functional>
#include <vector>

#include <glm/vec3.hpp>

#include "Creature/LeashRope.h"
#include "Graphics/RenderModes.h"
#include "Graphics/WorldTriangles.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::components
{
struct CreatureLeash;
}

/// What the renderer draws of a creature's leash: the rope's ribbon facing the camera and its shadow flat on the land,
/// as world triangles already lit, in the leash texture. Only the vertices are made here; the renderer submits them.
namespace openblack::graphics::leash_draw
{

/// The rope's material: blended and alpha tested, writing depth, as the game draws the leash. Two-sided and tiling,
/// as the ribbon faces the camera and its texture repeats along it (inferred: the flags are not read yet).
inline constexpr render_modes::Material k_RopeMaterial {render_modes::Mode::TexturedChromaAlpha,
                                                        render_modes::k_TwoSided | render_modes::k_Tiling};
/// The shadow strip's: blended without writing depth, as the other shadows on the land (inferred)
inline constexpr render_modes::Material k_ShadowMaterial {render_modes::Mode::AlphaTexturedAlphaNoZWrite,
                                                          render_modes::k_TwoSided | render_modes::k_Tiling};

/// The light at a point of the rope, 0xAARRGGBB; its alpha is not used
using LightAt = std::function<uint32_t(const glm::vec3&)>;

/// The corners of a ribbon, in leash_rope::RibbonIndices' order
using Corners = std::array<world_triangles::Vertex, leash_rope::k_RibbonVertexCount>;

/// One rope as drawn this frame
struct Draw
{
	/// The rope, each corner in the light at it and fully opaque
	Corners rope;
	/// The shadow, black and faint, fading out at both ends
	Corners shadow;
	/// The rope's middle point, which it is sorted by among the blended things
	glm::vec3 middle;
};

/// The rope's ribbon facing an eye and its shadow on the land, lit
[[nodiscard]] Draw Build(const leash_rope::Rope& rope, const glm::vec3& eye, const leash_rope::GroundHeight& ground,
                         const LightAt& light);

/// Whether a creature's leash is drawn: it wears one, and its rope has been placed from its ends
[[nodiscard]] bool IsDrawn(const ecs::components::CreatureLeash& leash);

/// Every rope to draw this frame, none while the scripts hide the leashes. Looked up through the const registry, so a
/// land without a creature gains no storage.
[[nodiscard]] std::vector<const leash_rope::Rope*> Ropes(const ecs::Registry& registry, bool shown);

} // namespace openblack::graphics::leash_draw
