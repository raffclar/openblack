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

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include "3D/LandLightTable.h"

namespace openblack
{
class LandBlock;
} // namespace openblack

/// The original's software distance haze, one formula with three implementations (see docs/bw1-notes/rendering.md,
/// "Neblina de distancia"): the objects' (once per object, at its origin), the land's per vertex (in two versions) with
/// a class per block, and the parameters the light table sets every frame. The GPU side is assets/shaders/haze.sh,
/// with the same rounding.
///
/// Rounding of the original, kept here: the diffuse factor f = 256 - trunc((256 - k) t); the diffuse (c f) >> 8 per
/// byte with its alpha kept; the haze colour c t rounded to nearest, halves to even (the FPU's default mode); the
/// land's class 2 the colour packed truncated and f = k.
namespace openblack::graphics::haze
{

/// The haze parameters and the "Fog" detail key
struct Params
{
	bool on {false}; ///< the "Fog" detail key
	float nearDistance {15.0f};
	float farDistance {400.0f};
	float range {385.0f};        ///< far - near
	int k {256};                 ///< the diffuse factor at t = 1, in 1/256
	glm::vec3 colour {64.0f};    ///< R, G, B 0..255 (floats)
	uint32_t packed {0x404040u}; ///< trunc(b) | trunc(g) << 8 | trunc(r) << 16, alpha 0
};

/// The parameters from the haze of this frame's light table (near, far, r, g, b, k); `on` is the "Fog" detail key
[[nodiscard]] Params FromTable(const LandLightTable::Haze& haze, bool on) noexcept;
/// The haze of this frame in one place: land_light::CurrentTable() (the last Build) and the "Fog" detail
/// key of the config. Off without a config.
[[nodiscard]] Params Frame() noexcept;

/// The depth along the camera axis, the z column of the original's world-to-camera matrix; in openblack the view
/// matrix's z row (the view looks down +z, Camera.h)
[[nodiscard]] float Depth(const glm::mat4& view, const glm::vec3& point) noexcept;

/// The FPU's default rounding (to nearest, halves to even)
[[nodiscard]] int RoundHalfEven(float value) noexcept;

/// t of the objects' haze and the land's class 1: (min(max(z, near), far) - near) / range, a true division. The
/// objects' haze has already returned when z < near, which gives the same t = 0
[[nodiscard]] float HazeFraction(const Params& params, float depth) noexcept;
/// f = 256 - trunc((256 - k) t); the diffuse is scaled only when f < 256
[[nodiscard]] int Factor(const Params& params, float t) noexcept;
/// (c f) >> 8 per RGB byte, alpha kept; nothing when f >= 256 (an unsigned comparison)
[[nodiscard]] uint32_t ScaleDiffuse(uint32_t argb, int f) noexcept;
/// The haze colour c t of every channel rounded to nearest (RoundHalfEven), as 0x00RRGGBB
[[nodiscard]] uint32_t Colour(const Params& params, float t) noexcept;
/// a + b per channel, each capped at 0xFF; alpha of neither
[[nodiscard]] uint32_t AddSaturated(uint32_t a, uint32_t b) noexcept;

/// The objects' haze: with the haze off or the depth < near (strict) it returns the
/// specular untouched; else it scales *diffuse (when not null) by f and returns the haze colour with alpha 0xFF, added
/// with saturation to a non-zero specular (alpha 0xFF either way). Once per object, at the point the caller passes.
[[nodiscard]] uint32_t ApplyObject(const Params& params, float depth, uint32_t specular, uint32_t* diffuse) noexcept;

/// A land block's class, from the camera depth of the 8 corners of the block's box: bit 2 for a corner past far, bit 1
/// for one in (near, far]; class 0 with the haze off or no bit, 1 with any bit 1, else 2 (the whole block at full haze;
/// also, (inferred) never seen, a block with corners <= near and > far and none between)
[[nodiscard]] int BlockClass(const Params& params, const std::array<float, 8>& cornerDepths) noexcept;
/// The 8 corners of a block's box, fed one by one to the depth: x and z the block's map position + 80 +- 80; y from
/// h = highest altitude x 0.67: centre h / 2 +- h / 2, or with the land reflection on centre 0 +- h (the mirrored
/// land). (inferred) the height is LNDBlock::highestAltitude and the flag the LandRef detail key
/// (DetailLevel::landReflection). The order of the corners does not change BlockClass
[[nodiscard]] std::array<glm::vec3, 8> BlockCorners(glm::vec2 mapPosition, float highestAltitude, bool landRef) noexcept;
/// The class of one land block: BlockClass of the depths (Depth) of its BlockCorners
[[nodiscard]] int BlockClassOf(const Params& params, const glm::mat4& view, const LandBlock& block, bool landRef) noexcept;
/// The land's haze per vertex: class 0 nothing; class 2 f = k and the packed colour (truncated, alpha 0); class 1 t
/// from the clamped depth, f and the rounded colour. The specular gets the colour added with saturation, keeping its
/// own alpha, or becomes it (alpha 0) when it was 0; then the diffuse (c f) >> 8 when f < 256. Returns them through the
/// references.
void ApplyVertex(const Params& params, int blockClass, float depth, uint32_t& diffuse, uint32_t& specular) noexcept;

/// u_haze (x near, y far, z k, w on) and u_hazeColour (rgb the colour 0..255, w: unused) for haze.sh
[[nodiscard]] std::array<glm::vec4, 2> Uniforms(const Params& params) noexcept;

} // namespace openblack::graphics::haze
