/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TattooPictures.h"

#include <Bitmap.h>

using namespace openblack;

std::string tattoo_pictures::FileName(uint32_t design)
{
	return "Symbol" + std::to_string(design + 1) + ".bmp";
}

std::optional<creature_tattoo::Picture> tattoo_pictures::FromBitmap(std::span<const uint8_t> bytes)
{
	if (bytes.size() < k_SmallestFile)
	{
		return std::nullopt;
	}
	const auto image = rawimage::DecodeBitmap(bytes);
	if (!image.has_value())
	{
		return std::nullopt;
	}
	return creature_tattoo::PictureFromImage(image->pixels, image->width, image->height);
}

void tattoo_pictures::Read(creature_tattoo::Pictures& pictures,
                           const std::function<std::optional<std::vector<uint8_t>>(const std::string& file)>& reader)
{
	for (uint32_t design = 0; design < pictures.size(); ++design)
	{
		const auto bytes = reader(FileName(design));
		if (!bytes.has_value())
		{
			continue;
		}
		if (auto picture = FromBitmap(*bytes); picture.has_value())
		{
			pictures.at(design) = *picture;
		}
	}
}
