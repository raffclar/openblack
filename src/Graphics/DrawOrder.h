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

#include <bgfx/defines.h>

#include "Graphics/RenderPass.h"

/// The order of the opaque draws within a program. bgfx sorts the draws of the scene's views by program and then by the
/// depth each draw is given, keeping the order they came in where both are the same. Vulkan makes a new set of bindings
/// whenever a draw binds other textures than the draw before it, and holds about a thousand such sets a frame: drawn in
/// the order the models come, the meshes that share a texture are scattered among the rest, and a busy land runs out.
/// Draws whose order can't change the picture are given a depth that gathers those binding the same textures.
namespace openblack::graphics::draw_order
{

/// Whether a draw may go anywhere among its program's draws without changing the picture: in the object shadows,
/// every caster writes the same value; in the scene's views, an opaque draw that writes its depth and keeps only what
/// is nearer than what is there already shows the same whatever is drawn before or after it, but where two surfaces
/// have exactly the same depth, where the first drawn stays (a pixel or two of the most distant models, whose order
/// was as arbitrary before). Draws given a depth of their own, those that blend and those that don't write their depth
/// keep their order.
[[nodiscard]] constexpr bool Reorderable(RenderPass view, uint64_t state, uint32_t sortDepth) noexcept
{
	if (sortDepth != 0)
	{
		return false;
	}
	if (view == RenderPass::ObjectShadow)
	{
		return true;
	}
	if (view != RenderPass::Main && view != RenderPass::Reflection)
	{
		return false;
	}
	return (state & BGFX_STATE_BLEND_MASK) == 0 && (state & BGFX_STATE_WRITE_Z) != 0 &&
	       (state & BGFX_STATE_DEPTH_TEST_MASK) == BGFX_STATE_DEPTH_TEST_GREATER;
}

/// The textures that change from one model's draw to the next
struct Bindings
{
	/// The bgfx texture handles' indices, or UINT16_MAX for none
	uint16_t diffuse {UINT16_MAX};
	uint16_t lightmap {UINT16_MAX};
	/// The snow's textures are bound, or the program's defaults are
	bool snowed {false};
};

/// The depth that puts draws of the same bindings next to each other. Its top bit is always set, so that the draws are
/// never given 0, the depth of the draws keeping their order; bgfx holds at most 4096 textures, which fit in 14 bits.
[[nodiscard]] constexpr uint32_t GroupKey(const Bindings& bindings) noexcept
{
	constexpr uint32_t k_Grouped = 1u << 31u;
	constexpr uint32_t k_Snowed = 1u << 30u;
	const uint32_t diffuse = static_cast<uint16_t>(bindings.diffuse + 1u);
	const uint32_t lightmap = static_cast<uint16_t>(bindings.lightmap + 1u) & 0x3FFFu;
	return k_Grouped | (bindings.snowed ? k_Snowed : 0u) | (lightmap << 16u) | diffuse;
}

} // namespace openblack::graphics::draw_order
