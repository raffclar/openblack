/******************************************************************************
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

#include <span>
#include <vector>

namespace openblack
{
/// A 16-bit image file (the sky's .555 layers): a header of u32s (the width at 1, the height at 2) and the
/// 16-bit texels from the 5th u32
class Bitmap16B
{
public:
	/// Parses the bytes of a whole file (read through the resource caches)
	explicit Bitmap16B(std::span<const uint8_t> fileData);

	[[nodiscard]] unsigned int Width() const { return _width; }
	[[nodiscard]] unsigned int Height() const { return _height; }
	[[nodiscard]] const uint16_t* Data() const { return _data.data(); }
	[[nodiscard]] size_t Size() const { return _data.size() * sizeof(uint16_t); }

private:
	unsigned int _width;
	unsigned int _height;
	std::vector<uint16_t> _data;
};

} // namespace openblack
