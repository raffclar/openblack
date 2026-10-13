/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ChallengePictures.h"

#include <cstddef>

#include <algorithm>
#include <array>

#include <fmt/format.h>

#include "ECS/Components/ChallengePicture.h"
#include "ECS/Registry.h"

using namespace openblack::ecs;
using openblack::ecs::components::ChallengePicture;

namespace
{
constexpr uint32_t k_BytesPerScreenPixel = 4;
constexpr uint32_t k_BytesPerPixel = 3;
/// The picture's shape: 4:3
constexpr uint32_t k_ShapeWide = 4;
constexpr uint32_t k_ShapeHigh = 3;
/// Two pixels are blended keeping this much of the first, out of 255
constexpr uint32_t k_BlendKeep = 128;
constexpr uint32_t k_BlendWhole = 255;

using Rgb = std::array<uint8_t, k_BytesPerPixel>;

/// A screen pixel, BGRA, as red, green and blue
Rgb ScreenPixel(const openblack::graphics::CapturedFrame& screen, uint32_t x, uint32_t row)
{
	const auto offset = static_cast<size_t>(row) * screen.pitch + static_cast<size_t>(x) * k_BytesPerScreenPixel;
	return {screen.bgra[offset + 2], screen.bgra[offset + 1], screen.bgra[offset]};
}

/// Blends b into a, keeping a little more than half of a, as the game blends 8-bit colours
uint8_t Blend(uint8_t a, uint8_t b)
{
	return static_cast<uint8_t>((b * (k_BlendWhole - k_BlendKeep) + a * k_BlendKeep) / k_BlendWhole);
}

Rgb Blend(const Rgb& a, const Rgb& b)
{
	return {Blend(a[0], b[0]), Blend(a[1], b[1]), Blend(a[2], b[2])};
}

Rgb PixelAt(std::span<const uint8_t> picture, uint32_t width, uint32_t x, uint32_t y)
{
	const auto offset = (static_cast<size_t>(y) * width + x) * k_BytesPerPixel;
	return {picture[offset], picture[offset + 1], picture[offset + 2]};
}
} // namespace

challenge_pictures::Region challenge_pictures::PictureRegion(uint32_t width, uint32_t height)
{
	if (width * k_ShapeHigh > height * k_ShapeWide)
	{
		const auto wide = height * k_ShapeWide / k_ShapeHigh;
		return {.x = (width - wide) / 2, .y = 0, .width = wide, .height = height};
	}
	const auto high = width * k_ShapeHigh / k_ShapeWide;
	return {.x = 0, .y = (height - high) / 2, .width = width, .height = high};
}

std::vector<uint8_t> challenge_pictures::Take(const graphics::CapturedFrame& screen)
{
	const auto region = PictureRegion(screen.width, screen.height);
	if (region.width == 0 || region.height == 0 || screen.bgra.size() < static_cast<size_t>(screen.pitch) * screen.height ||
	    screen.pitch < screen.width * k_BytesPerScreenPixel)
	{
		return {};
	}
	// The screen is stepped through evenly, the steps cut down to whole pixels
	const auto rowStep = static_cast<float>(region.height) / static_cast<float>(k_Height);
	const auto columnStep = static_cast<float>(region.width) / static_cast<float>(k_Width);

	std::vector<uint8_t> picture(static_cast<size_t>(k_Width) * k_Height * k_BytesPerPixel);
	auto out = picture.begin();
	for (uint32_t y = 0; y < k_Height; ++y)
	{
		const auto down = region.y + static_cast<uint32_t>(static_cast<double>(y) * rowStep);
		const auto row = screen.yflip ? screen.height - 1 - down : down;
		auto previous = ScreenPixel(screen, region.x, row);
		for (uint32_t x = 0; x < k_Width; ++x)
		{
			const auto across = region.x + static_cast<uint32_t>(static_cast<double>(x) * columnStep);
			const auto pixel = ScreenPixel(screen, across, row);
			for (size_t channel = 0; channel < k_BytesPerPixel; ++channel)
			{
				*out++ = static_cast<uint8_t>((previous.at(channel) + pixel.at(channel)) / 2);
			}
			previous = pixel;
		}
	}

	// Softened from the picture as picked: each pixel only ever reads the ones after it
	for (uint32_t y = 0; y + 1 < k_Height; ++y)
	{
		for (uint32_t x = 0; x + 1 < k_Width; ++x)
		{
			const auto top = Blend(PixelAt(picture, k_Width, x, y), PixelAt(picture, k_Width, x + 1, y));
			const auto bottom = Blend(PixelAt(picture, k_Width, x, y + 1), PixelAt(picture, k_Width, x + 1, y + 1));
			const auto blended = Blend(top, bottom);
			const auto offset = (static_cast<size_t>(y) * k_Width + x) * k_BytesPerPixel;
			std::copy(blended.begin(), blended.end(), picture.begin() + static_cast<std::ptrdiff_t>(offset));
		}
	}
	return picture;
}

std::vector<uint8_t> challenge_pictures::Shrink(std::span<const uint8_t> full)
{
	if (full.size() < static_cast<size_t>(k_Width) * k_Height * k_BytesPerPixel)
	{
		return {};
	}
	std::vector<uint8_t> small(static_cast<size_t>(k_SmallWidth) * k_SmallHeight * k_BytesPerPixel);
	auto out = small.begin();
	for (uint32_t y = 0; y < k_SmallHeight; ++y)
	{
		for (uint32_t x = 0; x < k_SmallWidth; ++x)
		{
			const auto left = PixelAt(full, k_Width, x * 2, y * 2);
			const auto right = PixelAt(full, k_Width, x * 2 + 1, y * 2);
			for (size_t channel = 0; channel < k_BytesPerPixel; ++channel)
			{
				*out++ = static_cast<uint8_t>((left.at(channel) + right.at(channel)) / 2);
			}
		}
	}
	return small;
}

std::string challenge_pictures::Ask(ChallengePicture& picture, entt::entity record)
{
	auto name = fmt::format("{}challenge-picture/{}/{}", graphics::CapturedFrames::k_Prefix, static_cast<uint32_t>(record),
	                        picture.asked);
	++picture.asked;
	picture.waiting = name;
	return name;
}

bool challenge_pictures::AnyWaiting(const Registry& registry)
{
	bool waiting = false;
	registry.Each<const ChallengePicture>(
	    [&waiting](entt::entity, const ChallengePicture& picture) { waiting = waiting || picture.waiting.has_value(); });
	return waiting;
}

void challenge_pictures::Collect(Registry& registry, const Screens& screens)
{
	registry.Each<ChallengePicture>([&screens](entt::entity, ChallengePicture& picture) {
		if (!picture.waiting.has_value())
		{
			return;
		}
		if (const auto screen = screens.take ? screens.take(*picture.waiting) : std::nullopt; screen.has_value())
		{
			picture.waiting.reset();
			if (auto full = Take(*screen); !full.empty())
			{
				picture.small = Shrink(full);
				picture.full = std::move(full);
				++picture.taken;
			}
			return;
		}
		if (screens.failed && screens.failed(*picture.waiting))
		{
			picture.waiting.reset();
		}
	});
}
