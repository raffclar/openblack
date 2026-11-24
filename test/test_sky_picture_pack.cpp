/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <algorithm>
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

/// Adds a number to a file as 4 bytes, the high byte first, as a PNG file keeps it
void PutBigEndian(std::vector<uint8_t>& out, uint32_t value)
{
	for (int shift = 24; shift >= 0; shift -= 8)
	{
		out.push_back(static_cast<uint8_t>(value >> shift));
	}
}

/// The check sum of the chunks of a PNG file
uint32_t Crc32(const std::vector<uint8_t>& bytes, size_t start)
{
	uint32_t crc = 0xFFFFFFFFu;
	for (size_t i = start; i < bytes.size(); ++i)
	{
		crc ^= bytes[i];
		for (int bit = 0; bit < 8; ++bit)
		{
			crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
		}
	}
	return ~crc;
}

/// A chunk of a PNG file: its length, its type, its data and its check sum
void PutChunk(std::vector<uint8_t>& file, const char* type, const std::vector<uint8_t>& data)
{
	PutBigEndian(file, static_cast<uint32_t>(data.size()));
	const auto start = file.size();
	file.insert(file.end(), type, type + 4);
	file.insert(file.end(), data.begin(), data.end());
	PutBigEndian(file, Crc32(file, start));
}

/// A PNG file of one colour with 16 bits for each channel. The data is not compressed.
std::vector<uint8_t> OneColourPng16(uint32_t side, uint16_t red, uint16_t green, uint16_t blue)
{
	std::vector<uint8_t> rows;
	for (uint32_t y = 0; y < side; ++y)
	{
		rows.push_back(0);
		for (uint32_t x = 0; x < side; ++x)
		{
			for (const uint16_t value : {red, green, blue, uint16_t {0}})
			{
				rows.push_back(static_cast<uint8_t>(value >> 8));
				rows.push_back(static_cast<uint8_t>(value & 0xFF));
			}
		}
	}
	// A zlib stream of stored blocks, then the Adler-32 sum of the data
	std::vector<uint8_t> stream {0x78, 0x01};
	constexpr size_t k_Block = 65535;
	for (size_t start = 0; start < rows.size(); start += k_Block)
	{
		const auto length = static_cast<uint16_t>(std::min(k_Block, rows.size() - start));
		stream.push_back(start + length == rows.size() ? 1 : 0);
		for (const uint16_t value : {length, static_cast<uint16_t>(~length)})
		{
			stream.push_back(static_cast<uint8_t>(value & 0xFF));
			stream.push_back(static_cast<uint8_t>(value >> 8));
		}
		stream.insert(stream.end(), rows.begin() + static_cast<std::ptrdiff_t>(start),
		              rows.begin() + static_cast<std::ptrdiff_t>(start + length));
	}
	uint32_t low = 1;
	uint32_t high = 0;
	for (const auto byte : rows)
	{
		low = (low + byte) % 65521;
		high = (high + low) % 65521;
	}
	PutBigEndian(stream, (high << 16) | low);

	std::vector<uint8_t> file {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
	std::vector<uint8_t> header;
	PutBigEndian(header, side);
	PutBigEndian(header, side);
	// 16 bits, red, green, blue and alpha, no interlace
	header.insert(header.end(), {16, 6, 0, 0, 0});
	PutChunk(file, "IHDR", header);
	PutChunk(file, "IDAT", stream);
	PutChunk(file, "IEND", {});
	return file;
}

/// The value of a positive half float
float FromHalf(uint16_t half)
{
	const int exponent = (half >> 10) & 0x1F;
	const auto mantissa = static_cast<float>(half & 0x3FF);
	if (exponent == 0)
	{
		return std::ldexp(mantissa, -24);
	}
	return std::ldexp(1.0f + (mantissa / 1024.0f), exponent - 15);
}

/// The value of a half float in the texels of a pack
float HalfAt(const std::vector<uint8_t>& texels, size_t channel)
{
	uint16_t half = 0;
	std::memcpy(&half, &texels.at(channel * sizeof(half)), sizeof(half));
	return FromHalf(half);
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

TEST(SkyPicturePack, HalfFloatsAreTheNearestValues)
{
	EXPECT_EQ(sky_picture_pack::HalfFromFloat(0.0f), 0x0000);
	EXPECT_EQ(sky_picture_pack::HalfFromFloat(1.0f), 0x3C00);
	EXPECT_EQ(sky_picture_pack::HalfFromFloat(0.5f), 0x3800);
	EXPECT_EQ(sky_picture_pack::HalfFromFloat(65504.0f), 0x7BFF);
	EXPECT_EQ(sky_picture_pack::HalfFromFloat(1.0e6f), 0x7C00);
	// The smallest half float under the normal values, and half of it rounds to even: to zero
	EXPECT_EQ(sky_picture_pack::HalfFromFloat(std::ldexp(1.0f, -24)), 0x0001);
	EXPECT_EQ(sky_picture_pack::HalfFromFloat(std::ldexp(1.0f, -25)), 0x0000);
	// Each value of 16 bits from 0 to 1 comes back within half a step of a half float
	for (uint32_t value = 0; value <= 65535; value += 7)
	{
		const float unit = static_cast<float>(value) / 65535.0f;
		const float back = FromHalf(sky_picture_pack::HalfFromFloat(unit));
		EXPECT_LE(std::abs(back - unit), std::max(unit, std::ldexp(1.0f, -14)) / 2048.0f) << value;
	}
}

TEST(SkyPicturePack, APictureOf16BitsKeepsAllItsBits)
{
	const auto file = OneColourPng16(256, 0x1234, 0x8001, 0xFFFF);
	const auto picture = sky_picture_pack::Decode(file);
	ASSERT_TRUE(picture.has_value());
	EXPECT_EQ(picture->bits, 16);
	EXPECT_EQ(picture->size, (Size {256, 256}));
	ASSERT_EQ(picture->texels.size(), 256u * 256u * sky_picture_pack::k_TexelBytes * sizeof(uint16_t));
	std::array<uint16_t, 4> texel {};
	std::memcpy(texel.data(), picture->texels.data(), sizeof(texel));
	EXPECT_EQ(texel.at(0), 0x1234);
	EXPECT_EQ(texel.at(1), 0x8001);
	EXPECT_EQ(texel.at(2), 0xFFFF);
	// The alpha of the file is not used: each texel is opaque
	EXPECT_EQ(texel.at(3), 0xFFFF);

	const auto eight = sky_picture_pack::Decode(OneColourPng(256, 256, 1, 2, 3));
	ASSERT_TRUE(eight.has_value());
	EXPECT_EQ(eight->bits, 8);
}

TEST(SkyPicturePack, APackWithAPictureOf16BitsKeepsHalfFloats)
{
	FakeFolder folder(256);
	// Between two steps of 8 bits: a picture of 8 bits cannot keep this value
	constexpr uint16_t k_Between = (100 * 257) + 128;
	folder.Put("Sky_Ntrl_Night.png", OneColourPng16(256, k_Between, 0, 65535));
	const auto loaded = sky_picture_pack::Load(folder.Reader(), k_LargeDevice);
	ASSERT_TRUE(loaded.pack.has_value());
	EXPECT_TRUE(loaded.pack->halfFloats);
	constexpr size_t k_LayerTexels = 256 * 256;
	ASSERT_EQ(loaded.pack->texels.size(), k_LayerTexels * sky_picture_pack::k_HalfTexelBytes * 9);
	constexpr size_t k_LayerChannels = k_LayerTexels * 4;
	// The picture of 16 bits keeps its step between the steps of 8 bits
	const auto between = HalfAt(loaded.pack->texels, 3 * k_LayerChannels);
	EXPECT_NEAR(between, static_cast<float>(k_Between) / 65535.0f, 1.0f / 4096.0f);
	EXPECT_GT(between, 100.0f / 255.0f);
	EXPECT_LT(between, 101.0f / 255.0f);
	EXPECT_EQ(HalfAt(loaded.pack->texels, (3 * k_LayerChannels) + 1), 0.0f);
	EXPECT_EQ(HalfAt(loaded.pack->texels, (3 * k_LayerChannels) + 2), 1.0f);
	EXPECT_EQ(HalfAt(loaded.pack->texels, (3 * k_LayerChannels) + 3), 1.0f);
	// The pictures of 8 bits in the same pack become half floats of the same values
	for (const size_t layer : {0u, 8u})
	{
		const auto first = layer * k_LayerChannels;
		EXPECT_NEAR(HalfAt(loaded.pack->texels, first), static_cast<float>(layer * 10) / 255.0f, 1.0f / 2048.0f);
		EXPECT_NEAR(HalfAt(loaded.pack->texels, first + 2), static_cast<float>((layer * 10) + 2) / 255.0f, 1.0f / 2048.0f);
		EXPECT_EQ(HalfAt(loaded.pack->texels, first + 3), 1.0f);
	}
}

TEST(SkyPicturePack, APackOf8BitsStaysAt8Bits)
{
	const FakeFolder folder(256);
	const auto loaded = sky_picture_pack::Load(folder.Reader(), k_LargeDevice);
	ASSERT_TRUE(loaded.pack.has_value());
	EXPECT_FALSE(loaded.pack->halfFloats);
}

TEST(SkyPicturePack, APackOf16BitsHasASmallerLargestSide)
{
	// The check of the sizes accepts the side, but a pack of half floats keeps twice the bytes
	constexpr uint32_t k_Side = sky_picture_pack::k_LargestWideSide + sky_picture_pack::k_SideStep;
	FakeFolder folder(256);
	const auto eight = OneColourPng(k_Side, k_Side, 1, 2, 3);
	for (const auto& file : sky_picture_pack::PictureFiles())
	{
		folder.Put(file, eight);
	}
	folder.Put("sky_good_dusk.png", OneColourPng16(k_Side, 1, 2, 3));
	const auto loaded = sky_picture_pack::Load(folder.Reader(), k_LargeDevice);
	EXPECT_FALSE(loaded.pack.has_value());
	EXPECT_EQ(loaded.problem, Problem::TooLarge);
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

TEST(SkyPicturePack, ThePackShowsOnlyWhenTheSettingIsOnAndThePackLoaded)
{
	EXPECT_TRUE(sky_picture_pack::ShowsPack(true, true, false));
	EXPECT_FALSE(sky_picture_pack::ShowsPack(false, true, false));
	// With no pack the game's own pictures show, with no error
	EXPECT_FALSE(sky_picture_pack::ShowsPack(true, false, false));
	EXPECT_FALSE(sky_picture_pack::ShowsPack(true, true, true));
}
