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
#include <optional>
#include <vector>

#include "GraphicsHandle.h"

namespace openblack::graphics
{

/// The two triangles of a quad, as indices of its four corners
using QuadPattern = std::array<uint16_t, 6>;

/// The indices that join each four corners of a list into the two triangles of a quad by the pattern, for this count of
/// quads. Quad n uses the corners 4n to 4n + 3.
[[nodiscard]] std::vector<uint32_t> QuadIndexList(const QuadPattern& pattern, uint32_t quads);

/// The count of quads that a buffer holds when it must hold at least this count: a power of two, at least
/// k_LeastQuadCapacity. Then the buffer grows only a few times.
[[nodiscard]] uint32_t QuadCapacity(uint32_t quads);
inline constexpr uint32_t k_LeastQuadCapacity = 1024;

/// A static buffer of indices for the draws of lists of quads. Each quad of such a list has only its four corners in the
/// vertex data of the frame. The draws share these indices, so that a frame sends fewer bytes to the GPU.
class QuadIndices
{
public:
	explicit QuadIndices(const QuadPattern& pattern);
	~QuadIndices();
	QuadIndices(const QuadIndices&) = delete;
	QuadIndices& operator=(const QuadIndices&) = delete;
	QuadIndices(QuadIndices&&) = delete;
	QuadIndices& operator=(QuadIndices&&) = delete;

	/// Sets the indices of this count of quads for the next draw of the main encoder. The buffer grows when it holds
	/// fewer quads. False when the graphics library has no buffer for them.
	bool Bind(uint32_t quads);

private:
	QuadPattern _pattern;
	std::optional<IndexBufferHandle> _buffer;
	uint32_t _capacity {0};
};

} // namespace openblack::graphics
