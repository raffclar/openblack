/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <array>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <stb_image_write.h>

#include "3D/SkyDome.h"
#include "3D/SkyPicturePack.h"
#include "Graphics/SkyDomeTarget.h"

using namespace openblack;
using sky_picture_pack::Problem;
using sky_picture_pack::Size;

namespace
{
/// A PNG file of one colour, with no alpha in its texels
std::vector<uint8_t> OneColourPng(uint32_t width, uint32_t height, uint8_t red, uint8_t green, uint8_t blue)
{
	std::vector<uint8_t> texels;
	texels.reserve(static_cast<size_t>(width) * height * 4);
	for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i)
	{
		texels.insert(texels.end(), {red, green, blue, 0});
	}
	std::vector<uint8_t> file;
	stbi_write_png_to_func(
	    [](void* context, void* data, int size) {
		    auto& out = *static_cast<std::vector<uint8_t>*>(context);
		    const auto* bytes = static_cast<const uint8_t*>(data);
		    out.insert(out.end(), bytes, bytes + size);
	    },
	    &file, static_cast<int>(width), static_cast<int>(height), 4, texels.data(), static_cast<int>(width * 4));
	return file;
}

/// A fake folder of a pack: each picture of the side has the colour of its place in the pack
class FakeFolder
{
public:
	explicit FakeFolder(uint32_t side)
	{
		const auto files = sky_picture_pack::PictureFiles();
		for (size_t i = 0; i < files.size(); ++i)
		{
			const auto level = static_cast<uint8_t>(i * 10);
			_files[files.at(i)] =
			    OneColourPng(side, side, level, static_cast<uint8_t>(level + 1), static_cast<uint8_t>(level + 2));
		}
	}

	void Remove(const std::string& file) { _files.erase(file); }
	void Put(const std::string& file, std::vector<uint8_t> bytes) { _files[file] = std::move(bytes); }

	[[nodiscard]] sky_picture_pack::Reader Reader() const
	{
		return [this](std::string_view file) -> std::optional<std::vector<uint8_t>> {
			const auto found = _files.find(std::string(file));
			if (found == _files.end())
			{
				return std::nullopt;
			}
			return found->second;
		};
	}

private:
	std::map<std::string, std::vector<uint8_t>> _files;
};

constexpr uint32_t k_LargeDevice = 16384;
} // namespace

TEST(SkyPicturePack, ThePicturesHaveTheNamesOfTheGamesPicturesAsPng)
{
	const auto files = sky_picture_pack::PictureFiles();
	ASSERT_EQ(files.size(), 9u);
	EXPECT_EQ(files.at(0), "sky_good_night.png");
	EXPECT_EQ(files.at(2), "sky_good_day.png");
	EXPECT_EQ(files.at(3), "Sky_Ntrl_Night.png");
	EXPECT_EQ(files.at(4), "Sky_Ntrl_Dusk.png");
	EXPECT_EQ(files.at(8), "sky_evil_day.png");
}

TEST(SkyPicturePack, SizesAreSquaresOfAWholeMultipleOf256)
{
	const std::vector<Size> good(9, Size {1024, 1024});
	const auto checked = sky_picture_pack::CheckSizes(good, k_LargeDevice);
	EXPECT_FALSE(checked.problem.has_value());
	EXPECT_EQ(checked.side, 1024u);

	const std::vector<Size> original(9, Size {256, 256});
	EXPECT_EQ(sky_picture_pack::CheckSizes(original, k_LargeDevice).side, 256u);

	auto notSquare = good;
	notSquare.at(3) = {1024, 512};
	const auto square = sky_picture_pack::CheckSizes(notSquare, k_LargeDevice);
	EXPECT_EQ(square.problem, Problem::NotSquare);
	EXPECT_EQ(square.picture, 3u);
	EXPECT_EQ(square.side, 0u);

	auto notWhole = good;
	notWhole.at(5) = {300, 300};
	const auto whole = sky_picture_pack::CheckSizes(notWhole, k_LargeDevice);
	EXPECT_EQ(whole.problem, Problem::NotWholeStep);
	EXPECT_EQ(whole.picture, 5u);

	const std::vector<Size> empty(9, Size {0, 0});
	EXPECT_EQ(sky_picture_pack::CheckSizes(empty, k_LargeDevice).problem, Problem::NotWholeStep);

	auto differ = good;
	differ.at(8) = {512, 512};
	const auto sides = sky_picture_pack::CheckSizes(differ, k_LargeDevice);
	EXPECT_EQ(sides.problem, Problem::SidesDiffer);
	EXPECT_EQ(sides.picture, 8u);
}

TEST(SkyPicturePack, TheTargetOfThreeSidesMustFitTheDevice)
{
	// A device of 4096 texels keeps a target of 3 x 1024, not of 3 x 2048
	const std::vector<Size> fits(9, Size {1024, 1024});
	EXPECT_FALSE(sky_picture_pack::CheckSizes(fits, 4096).problem.has_value());
	const std::vector<Size> tooLarge(9, Size {2048, 2048});
	const auto checked = sky_picture_pack::CheckSizes(tooLarge, 4096);
	EXPECT_EQ(checked.problem, Problem::TooLarge);
	EXPECT_EQ(checked.picture, 0u);
	// The pack also has a largest side of its own
	const std::vector<Size> huge(9, Size {4352, 4352});
	EXPECT_EQ(sky_picture_pack::CheckSizes(huge, 65536).problem, Problem::TooLarge);
}

TEST(SkyPicturePack, LoadsTheLayersInTheOrderOfThePictures)
{
	const FakeFolder folder(256);
	const auto loaded = sky_picture_pack::Load(folder.Reader(), k_LargeDevice);
	ASSERT_TRUE(loaded.pack.has_value());
	EXPECT_FALSE(loaded.problem.has_value());
	EXPECT_EQ(loaded.pack->side, 256u);
	constexpr size_t k_Layer = 256 * 256 * sky_picture_pack::k_TexelBytes;
	ASSERT_EQ(loaded.pack->texels.size(), k_Layer * 9);
	for (size_t layer = 0; layer < 9; ++layer)
	{
		const auto* texel = &loaded.pack->texels.at(layer * k_Layer);
		EXPECT_EQ(texel[0], layer * 10);
		EXPECT_EQ(texel[1], layer * 10 + 1);
		EXPECT_EQ(texel[2], layer * 10 + 2);
		// The alpha of the file is not used: each texel is opaque
		EXPECT_EQ(texel[3], 0xFF);
	}
}

TEST(SkyPicturePack, AMissingPictureStopsTheLoad)
{
	FakeFolder folder(256);
	folder.Remove("Sky_Ntrl_Dusk.png");
	const auto loaded = sky_picture_pack::Load(folder.Reader(), k_LargeDevice);
	EXPECT_FALSE(loaded.pack.has_value());
	EXPECT_EQ(loaded.problem, Problem::MissingPicture);
	EXPECT_EQ(loaded.picture, 4u);
}

TEST(SkyPicturePack, ABadSizeStopsTheLoad)
{
	FakeFolder folder(256);
	folder.Put("sky_evil_dusk.png", OneColourPng(300, 300, 1, 2, 3));
	const auto loaded = sky_picture_pack::Load(folder.Reader(), k_LargeDevice);
	EXPECT_FALSE(loaded.pack.has_value());
	EXPECT_EQ(loaded.problem, Problem::NotWholeStep);
	EXPECT_EQ(loaded.picture, 7u);

	FakeFolder halves(256);
	halves.Put("sky_good_day.png", OneColourPng(256, 128, 1, 2, 3));
	EXPECT_EQ(sky_picture_pack::Load(halves.Reader(), k_LargeDevice).problem, Problem::NotSquare);
}

TEST(SkyPicturePack, AFileThatIsNotAPngStopsTheLoad)
{
	FakeFolder folder(256);
	folder.Put("sky_good_night.png", {'n', 'o', 't', ' ', 'a', ' ', 'p', 'n', 'g'});
	const auto loaded = sky_picture_pack::Load(folder.Reader(), k_LargeDevice);
	EXPECT_EQ(loaded.problem, Problem::UnreadablePicture);
	EXPECT_EQ(loaded.picture, 0u);
	EXPECT_FALSE(sky_picture_pack::Decode({}).has_value());
}

TEST(SkyDomeFullBlend, TheWeightsAreThoseOfTheGamesBlend)
{
	// 255 of 256 of the lower picture with no weight, and 255 of 256 of the upper one with all the weight
	EXPECT_EQ(sky_dome::FullWeights(0).x, 255.0f / 256.0f);
	EXPECT_EQ(sky_dome::FullWeights(0).y, 0.0f);
	EXPECT_EQ(sky_dome::FullWeights(255).x, 0.0f);
	EXPECT_EQ(sky_dome::FullWeights(255).y, 255.0f / 256.0f);
	for (int weight = 0; weight <= 255; ++weight)
	{
		const auto weights = sky_dome::FullWeights(static_cast<uint8_t>(weight));
		EXPECT_EQ(weights.x + weights.y, 255.0f / 256.0f) << weight;
		EXPECT_EQ(weights.y, static_cast<float>(weight) / 256.0f) << weight;
	}
}

TEST(SkyDomeFullBlend, TheBlendKeepsTheStepsOfTheLargerPictures)
{
	// Two 8-bit levels half and half: the result is not cut to a 5-bit step
	const float blended = sky_dome::BlendFull(100.0f / 255.0f, 200.0f / 255.0f, 128);
	EXPECT_NEAR(blended, ((100.0f * 127.0f) + (200.0f * 128.0f)) / (255.0f * 256.0f), 1e-6f);
	EXPECT_NE(blended * 31.0f, std::floor(blended * 31.0f));

	// For two 5-bit levels, the full blend is never below the game's blend, and less than two steps above it
	for (uint8_t lower = 0; lower < 32; ++lower)
	{
		for (uint8_t upper = 0; upper < 32; ++upper)
		{
			for (int weight = 0; weight <= 255; weight += 5)
			{
				const auto w = static_cast<uint8_t>(weight);
				const float full = sky_dome::BlendFull(lower, upper, w);
				const auto steps = static_cast<float>(sky_dome::BlendChannel(lower, upper, w));
				EXPECT_GE(full, steps);
				EXPECT_LT(full, steps + 2.0f);
			}
		}
	}
}

TEST(SkyDomeTarget, ThePackUsesHalfFloatsWhereTheDeviceDrawsThem)
{
	using graphics::TextureFormat;
	EXPECT_EQ(graphics::sky_dome_target::PackFormat(true, true), TextureFormat::RGBA16F);
	EXPECT_EQ(graphics::sky_dome_target::PackFormat(true, false), TextureFormat::RGBA16F);
	// Without half floats, as on WebGL 2 without the extension, the pack uses the target of the game's own pictures
	EXPECT_EQ(graphics::sky_dome_target::PackFormat(false, true), TextureFormat::RGB10A2);
	EXPECT_EQ(graphics::sky_dome_target::PackFormat(false, false), TextureFormat::RGBA8);
}
