/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include <ECS/ChallengePictures.h>
#include <ECS/Components/ChallengePicture.h>
#include <ECS/Registry.h>
#include <gtest/gtest.h>

namespace
{
namespace pictures = openblack::ecs::challenge_pictures;
using openblack::ecs::components::ChallengePicture;
using openblack::graphics::ScreenView;

/// The pixels of a test screen, and the view of them that the renderer gives
struct TestScreen
{
	uint32_t width {0};
	uint32_t height {0};
	bool yflip {false};
	std::vector<uint8_t> bgra;

	[[nodiscard]] ScreenView View() const
	{
		return {.width = width, .height = height, .pitch = width * 4, .yflip = yflip, .bgra = bgra};
	}
};

/// A screen whose pixels (red, green, blue) come from where they are, top row first
TestScreen Screen(uint32_t width, uint32_t height, const std::function<std::array<uint8_t, 3>(uint32_t, uint32_t)>& colour,
                  bool yflip = false)
{
	TestScreen frame {.width = width, .height = height, .yflip = yflip, .bgra = {}};
	const auto pitch = static_cast<size_t>(width) * 4;
	frame.bgra.resize(pitch * height);
	for (uint32_t y = 0; y < height; ++y)
	{
		const auto row = yflip ? height - 1 - y : y;
		for (uint32_t x = 0; x < width; ++x)
		{
			const auto [r, g, b] = colour(x, y);
			const auto offset = static_cast<size_t>(row) * pitch + static_cast<size_t>(x) * 4;
			frame.bgra[offset] = b;
			frame.bgra[offset + 1] = g;
			frame.bgra[offset + 2] = r;
			frame.bgra[offset + 3] = 255;
		}
	}
	return frame;
}

/// The picture in high detail of a test screen
std::vector<uint8_t> TakeOf(const TestScreen& screen)
{
	return pictures::Take(screen.View());
}

uint8_t Red(const std::vector<uint8_t>& picture, uint32_t width, uint32_t x, uint32_t y)
{
	return picture.at((static_cast<size_t>(y) * width + x) * 3);
}

uint8_t Green(const std::vector<uint8_t>& picture, uint32_t width, uint32_t x, uint32_t y)
{
	return picture.at((static_cast<size_t>(y) * width + x) * 3 + 1);
}
} // namespace

TEST(ChallengePictures, APictureIsOfTheLargestMiddle4By3PartOfTheScreen)
{
	const auto classic = pictures::PictureRegion(640, 480);
	EXPECT_EQ(classic.x, 0u);
	EXPECT_EQ(classic.y, 0u);
	EXPECT_EQ(classic.width, 640u);
	EXPECT_EQ(classic.height, 480u);

	const auto wide = pictures::PictureRegion(1920, 1080);
	EXPECT_EQ(wide.x, 240u);
	EXPECT_EQ(wide.width, 1440u);
	EXPECT_EQ(wide.height, 1080u);

	const auto tall = pictures::PictureRegion(1280, 1024);
	EXPECT_EQ(tall.y, 32u);
	EXPECT_EQ(tall.width, 1280u);
	EXPECT_EQ(tall.height, 960u);
}

TEST(ChallengePictures, AnEvenScreenGivesAnEvenPicture)
{
	const auto picture = TakeOf(Screen(640, 480, [](uint32_t, uint32_t) { return std::array<uint8_t, 3> {10, 20, 30}; }));
	ASSERT_EQ(picture.size(), static_cast<size_t>(pictures::k_Width) * pictures::k_Height * 3);
	for (size_t i = 0; i < picture.size(); i += 3)
	{
		ASSERT_EQ(picture[i], 10);
		ASSERT_EQ(picture[i + 1], 20);
		ASSERT_EQ(picture[i + 2], 30);
	}
}

TEST(ChallengePictures, EachPixelIsMeanedWithTheOneBeforeItThenSoftened)
{
	// Red on the right half of the screen only; one screen pixel per picture column
	const auto picture = TakeOf(Screen(256, 192, [](uint32_t x, uint32_t) {
		return std::array<uint8_t, 3> {static_cast<uint8_t>(x >= 128 ? 255 : 0), 0, 0};
	}));
	ASSERT_FALSE(picture.empty());
	// The last row stays as picked: the pixel at the edge is the mean of black and red
	EXPECT_EQ(Red(picture, pictures::k_Width, 127, 127), 0);
	EXPECT_EQ(Red(picture, pictures::k_Width, 128, 127), 127);
	EXPECT_EQ(Red(picture, pictures::k_Width, 129, 127), 255);
	// Softened, each pixel is blended with its neighbour to the right (keeping 128/255 of itself)
	EXPECT_EQ(Red(picture, pictures::k_Width, 126, 0), 0);
	EXPECT_EQ(Red(picture, pictures::k_Width, 127, 0), 63);
	EXPECT_EQ(Red(picture, pictures::k_Width, 128, 0), 190);
	EXPECT_EQ(Red(picture, pictures::k_Width, 129, 0), 255);
}

TEST(ChallengePictures, APictureReadBottomUpIsTheRightWayUp)
{
	const auto top = [](uint32_t, uint32_t y) {
		return std::array<uint8_t, 3> {0, static_cast<uint8_t>(y < 240 ? 255 : 0), 0};
	};
	const auto upright = TakeOf(Screen(640, 480, top));
	const auto flipped = TakeOf(Screen(640, 480, top, true));
	EXPECT_EQ(upright, flipped);
	EXPECT_EQ(Green(flipped, pictures::k_Width, 10, 0), 255);
	EXPECT_EQ(Green(flipped, pictures::k_Width, 10, 127), 0);
}

TEST(ChallengePictures, AWideScreenLeavesItsSidesOut)
{
	const auto picture = TakeOf(Screen(1920, 1080, [](uint32_t x, uint32_t) {
		return std::array<uint8_t, 3> {static_cast<uint8_t>(x < 240 || x >= 1680 ? 255 : 0), 0, 0};
	}));
	ASSERT_FALSE(picture.empty());
	for (size_t i = 0; i < picture.size(); i += 3)
	{
		ASSERT_EQ(picture[i], 0);
	}
}

TEST(ChallengePictures, TheSmallPictureMeansPairsOfEveryOtherRow)
{
	std::vector<uint8_t> full(static_cast<size_t>(pictures::k_Width) * pictures::k_Height * 3, 0);
	// Row 0: red 100 then 201 in the first pair; row 1 is skipped
	full[0] = 100;
	full[3] = 201;
	full[static_cast<size_t>(pictures::k_Width) * 3] = 255;
	const auto small = pictures::Shrink(full);
	ASSERT_EQ(small.size(), static_cast<size_t>(pictures::k_SmallWidth) * pictures::k_SmallHeight * 3);
	EXPECT_EQ(small[0], 150);
	EXPECT_EQ(small[static_cast<size_t>(pictures::k_SmallWidth) * 3], 0);
}

TEST(ChallengePictures, ARecordsPictureComesBackOnceTheScreenIsRead)
{
	openblack::ecs::Registry registry;
	const auto record = registry.Create();
	auto& picture = registry.Assign<ChallengePicture>(record);
	const auto name = pictures::Ask(picture, record);
	EXPECT_TRUE(name.starts_with(openblack::graphics::ScreenPictures::k_Prefix));
	EXPECT_NE(pictures::Ask(picture, record), name);
	const auto asked = *picture.waiting;

	pictures::Collect(registry,
	                  {.take = [](const std::string&) { return std::optional<std::vector<uint8_t>> {}; }, .failed = {}});
	EXPECT_TRUE(registry.Get<ChallengePicture>(record).waiting.has_value());
	EXPECT_TRUE(pictures::AnyWaiting(registry));

	pictures::Collect(registry,
	                  {.take =
	                       [&asked](const std::string& wanted) {
		                       return wanted == asked
		                                  ? std::optional(TakeOf(Screen(
		                                        640, 480, [](uint32_t, uint32_t) { return std::array<uint8_t, 3> {1, 2, 3}; })))
		                                  : std::nullopt;
	                       },
	                   .failed = {}});
	const auto& taken = registry.Get<ChallengePicture>(record);
	EXPECT_FALSE(taken.waiting.has_value());
	EXPECT_FALSE(pictures::AnyWaiting(registry));
	EXPECT_EQ(taken.taken, 1u);
	EXPECT_EQ(taken.full.size(), static_cast<size_t>(pictures::k_Width) * pictures::k_Height * 3);
	EXPECT_EQ(taken.small.size(), static_cast<size_t>(pictures::k_SmallWidth) * pictures::k_SmallHeight * 3);
}

TEST(ChallengePictures, APictureGivenUpLeavesTheOldOne)
{
	openblack::ecs::Registry registry;
	const auto record = registry.Create();
	auto& picture = registry.Assign<ChallengePicture>(record);
	picture.full = {1, 2, 3};
	[[maybe_unused]] const auto name = pictures::Ask(picture, record);
	pictures::Collect(registry, {.take = {}, .failed = [](const std::string&) { return true; }});
	const auto& kept = registry.Get<ChallengePicture>(record);
	EXPECT_FALSE(kept.waiting.has_value());
	EXPECT_EQ(kept.full, (std::vector<uint8_t> {1, 2, 3}));
	EXPECT_EQ(kept.taken, 0u);
}
