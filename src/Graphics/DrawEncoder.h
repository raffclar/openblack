/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <bitset>

#include "ShaderSamplers.h"

namespace bgfx
{
struct Encoder;
}

namespace openblack::graphics
{
/// The texture stages that have a texture bound on one bgfx encoder now. A stage set by a draw stays set, as bgfx keeps
/// it, until a submit or a discard drops the bindings, or the frame ends.
class BoundStages
{
public:
	/// bgfx's BGFX_DISCARD_BINDINGS
	static constexpr uint8_t k_DiscardBindings = 0x01;

	/// A texture was bound at this stage
	void Set(uint8_t stage) noexcept
	{
		if (stage < _set.size())
		{
			_set.set(stage);
		}
	}
	[[nodiscard]] bool IsSet(uint8_t stage) const noexcept { return stage < _set.size() && _set.test(stage); }
	/// A submit or discard with these bgfx discard flags happened: the bindings went if the flags drop them
	void Discarded(uint8_t flags) noexcept
	{
		if ((flags & k_DiscardBindings) != 0)
		{
			_set.reset();
		}
	}
	/// All bindings went, as at the end of a frame
	void Clear() noexcept { _set.reset(); }

private:
	std::bitset<shader_samplers::k_MaxStages> _set;
};

/// Where draws go: one bgfx encoder and the texture stages bound on it. The game's thread has one for bgfx's own
/// encoder. A thread that encodes draws at the same time uses one of its own, with an encoder that bgfx gives that
/// thread.
struct DrawEncoder
{
	bgfx::Encoder* encoder {nullptr};
	BoundStages stages;
};

} // namespace openblack::graphics
