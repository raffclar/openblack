/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BinkYuv.h"

using namespace openblack::bink;

void openblack::bink::ConvertPicture(const Picture& picture, PixelLayout layout, std::span<uint8_t> out) noexcept
{
	const uint32_t width = picture.y.width;
	const uint32_t height = picture.y.height;
	if (out.size() < static_cast<size_t>(width) * height * 4)
	{
		return;
	}
	for (uint32_t row = 0; row < height; ++row)
	{
		const auto ys = picture.y.pixels.subspan(static_cast<size_t>(row) * picture.y.stride, width);
		const auto us = picture.u.pixels.subspan(static_cast<size_t>(row / 2) * picture.u.stride, picture.u.width);
		const auto vs = picture.v.pixels.subspan(static_cast<size_t>(row / 2) * picture.v.stride, picture.v.width);
		auto line = out.subspan(static_cast<size_t>(row) * width * 4, static_cast<size_t>(width) * 4);
		for (uint32_t x = 0; x < width; ++x)
		{
			const auto colour = ToRgb(ys[x], us[x / 2], vs[x / 2]);
			auto pixel = line.subspan(static_cast<size_t>(x) * 4, 4);
			if (layout == PixelLayout::Rgba8)
			{
				pixel[0] = colour.r;
				pixel[1] = colour.g;
				pixel[2] = colour.b;
				pixel[3] = 0xFF;
			}
			else
			{
				pixel[0] = colour.b;
				pixel[1] = colour.g;
				pixel[2] = colour.r;
				pixel[3] = 0;
			}
		}
	}
}

std::array<std::array<int16_t, 4>, 512> openblack::bink::MakeShaderLookup() noexcept
{
	std::array<std::array<int16_t, 4>, 512> lookup {};
	for (size_t i = 0; i < 256; ++i)
	{
		lookup[i] = {k_LumaTable[i], k_RedFromV[i], k_GreenFromV[i], 0};
		lookup[256 + i] = {k_GreenFromU[i], k_BlueFromU[i], 0, 0};
	}
	return lookup;
}
