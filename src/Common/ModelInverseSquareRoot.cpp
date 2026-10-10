/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ModelInverseSquareRoot.h"

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>

float openblack::gutils::ModelInverseSquareRoot(float value)
{
	static const std::array<uint8_t, 128> k_Table = [] {
		std::array<uint8_t, 128> table {};
		for (uint32_t i = 0; i < table.size(); ++i)
		{
			const float x = std::bit_cast<float>((i | 0x1F80u) << 17);
			const float y = 1.0f / std::sqrt(x);
			table.at(i) = static_cast<uint8_t>((std::bit_cast<uint32_t>(y) + 0x2000u) >> 15);
		}
		table[0x40] = 0xFF;
		return table;
	}();
	const auto bits = std::bit_cast<uint32_t>(value);
	const uint32_t exponent = ((bits >> 23) & 0xFFu) << 22;
	const uint32_t guess =
	    ((0x5F000000u - exponent) & 0xFF800000u) | (static_cast<uint32_t>(k_Table.at((bits >> 17) & 0x7Fu)) << 15);
	const float y = std::bit_cast<float>(guess);
	float r = value * y;
	r = r * y;
	r = 3.0f - r;
	r = r * y;
	return r * 0.5f;
}
