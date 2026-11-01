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

#include "Visual/RenderModes.h"

/// The render state of a primitive of a see-through object. The object draws each material in the mode of the game's
/// second table of render modes (see render_modes::SeeThrough), and that mode sets the blending and the depth write.
namespace openblack::graphics::see_through
{

/// The bgfx blend and depth write bits of a see-through primitive whose mode has the description `mode`
[[nodiscard]] constexpr uint64_t State(const render_modes::ModeDesc& mode) noexcept
{
	uint64_t state = mode.zWrite ? BGFX_STATE_WRITE_Z : 0;
	switch (mode.blend)
	{
	case render_modes::Blend::Standard:
		state |= BGFX_STATE_BLEND_ALPHA;
		break;
	case render_modes::Blend::Additive:
		state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE);
		break;
	case render_modes::Blend::JustZ:
		state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ZERO, BGFX_STATE_BLEND_ONE);
		break;
	case render_modes::Blend::Disabled:
		break;
	}
	return state;
}

/// How the object shader tests the alpha of a see-through primitive
enum class AlphaTest : uint8_t
{
	/// The primitive is not see-through: the shader uses the usual alpha threshold
	None = 0,
	/// The mode has no alpha test: no fragment is dropped for its alpha
	Off = 1,
	/// The fragments with an alpha below the reference are dropped
	Reference = 2,
	/// The fragments with an alpha below the reference, lowered by the alpha of the instance, are dropped
	ScaledReference = 3,
};

/// The alpha test of a see-through primitive in the mode `mode`
[[nodiscard]] constexpr AlphaTest TestOf(render_modes::Mode mode) noexcept
{
	if (!render_modes::Desc(mode).alphaTest)
	{
		return AlphaTest::Off;
	}
	return render_modes::SeeThroughScalesRef(mode) ? AlphaTest::ScaledReference : AlphaTest::Reference;
}

} // namespace openblack::graphics::see_through
