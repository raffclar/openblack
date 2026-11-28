/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "QuadIndices.h"

#include <algorithm>
#include <bit>

#include <bgfx/bgfx.h>

#include "GraphicsHandleBgfx.h"

using namespace openblack::graphics;

std::vector<uint32_t> openblack::graphics::QuadIndexList(const QuadPattern& pattern, uint32_t quads)
{
	std::vector<uint32_t> indices;
	indices.reserve(static_cast<size_t>(quads) * pattern.size());
	for (uint32_t quad = 0; quad < quads; ++quad)
	{
		for (const auto corner : pattern)
		{
			indices.push_back((quad * 4) + corner);
		}
	}
	return indices;
}

uint32_t openblack::graphics::QuadCapacity(uint32_t quads)
{
	return std::bit_ceil(std::max(quads, k_LeastQuadCapacity));
}

QuadIndices::QuadIndices(const QuadPattern& pattern)
    : _pattern(pattern)
{
}

QuadIndices::~QuadIndices()
{
	if (_buffer.has_value())
	{
		bgfx::destroy(toBgfx(*_buffer));
	}
}

bool QuadIndices::Bind(uint32_t firstQuad, uint32_t quads)
{
	if (quads == 0)
	{
		return false;
	}
	const auto lastQuad = firstQuad + quads;
	if (lastQuad > _capacity)
	{
		// The graphics library frees the old buffer after the frame, so the draws of this frame that use it stay correct
		if (_buffer.has_value())
		{
			bgfx::destroy(toBgfx(*_buffer));
			_buffer.reset();
		}
		const auto capacity = QuadCapacity(lastQuad);
		const auto indices = QuadIndexList(_pattern, capacity);
		const auto handle = bgfx::createIndexBuffer(
		    bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint32_t))), BGFX_BUFFER_INDEX32);
		if (!bgfx::isValid(handle))
		{
			_capacity = 0;
			return false;
		}
		_buffer = fromBgfx(handle);
		_capacity = capacity;
	}
	const auto quadIndices = static_cast<uint32_t>(_pattern.size());
	bgfx::setIndexBuffer(toBgfx(*_buffer), firstQuad * quadIndices, quads * quadIndices);
	return true;
}
