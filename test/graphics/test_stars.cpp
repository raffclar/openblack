/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the stars of the night sky

#include <cmath>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <numbers>
#include <vector>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Graphics/Stars.h"

namespace stars = openblack::graphics::stars;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;

/// A catalogue file with the given stars, as the converter writes it
std::vector<uint8_t> FakeCatalogue(const std::vector<stars::CatalogueStar>& list)
{
	std::vector<uint8_t> bytes {'O', 'B', 'S', 'C'};
	const auto put = [&bytes](const auto& value) {
		const auto* raw = reinterpret_cast<const uint8_t*>(&value);
		bytes.insert(bytes.end(), raw, raw + sizeof(value));
	};
	put(uint32_t {1});
	put(static_cast<uint32_t>(list.size()));
	for (const auto& star : list)
	{
		put(star.rightAscension);
		put(star.declination);
		put(static_cast<int16_t>(std::lround(star.magnitude * 100.0f)));
		put(static_cast<int16_t>(star.colourIndex.has_value() ? std::lround(*star.colourIndex * 1000.0f) : -32768));
	}
	return bytes;
}

float Luminance(glm::vec3 rgb)
{
	return glm::dot(rgb, glm::vec3(0.2126f, 0.7152f, 0.0722f));
}

/// The record texels of a cell in a built texture
std::vector<glm::vec4> RecordsOf(const stars::StarTexture& texture, int cellNumber)
{
	const auto& index = texture.texels.at(static_cast<size_t>(stars::k_TextureWidth + cellNumber));
	const auto first = static_cast<size_t>((1 + stars::k_IndexRows) * stars::k_TextureWidth) + static_cast<size_t>(index.x);
	return {texture.texels.begin() + static_cast<ptrdiff_t>(first),
	        texture.texels.begin() + static_cast<ptrdiff_t>(first + static_cast<size_t>(index.y))};
}
} // namespace

TEST(Stars, TheCatalogueFileGivesItsStars)
{
	const auto bytes =
	    FakeCatalogue({{.rightAscension = 1.5f, .declination = -0.25f, .magnitude = 1.64f, .colourIndex = -0.22f},
	                   {.rightAscension = 0.1f, .declination = 1.55f, .magnitude = 2.02f, .colourIndex = {}}});
	const auto parsed = stars::ParseCatalogue(bytes);
	ASSERT_TRUE(parsed.has_value());
	ASSERT_EQ(parsed->size(), 2u);
	EXPECT_FLOAT_EQ(parsed->at(0).rightAscension, 1.5f);
	EXPECT_FLOAT_EQ(parsed->at(0).declination, -0.25f);
	EXPECT_NEAR(parsed->at(0).magnitude, 1.64f, 1e-5f);
	ASSERT_TRUE(parsed->at(0).colourIndex.has_value());
	EXPECT_NEAR(*parsed->at(0).colourIndex, -0.22f, 1e-5f);
	EXPECT_FALSE(parsed->at(1).colourIndex.has_value());
}

TEST(Stars, AFileThatIsNotACatalogueGivesNothing)
{
	auto bytes = FakeCatalogue({{.rightAscension = 1.0f, .declination = 0.0f, .magnitude = 3.0f, .colourIndex = 0.5f}});
	EXPECT_FALSE(stars::ParseCatalogue(std::vector<uint8_t>(bytes.begin(), bytes.end() - 1)).has_value());
	bytes.at(0) = 'X';
	EXPECT_FALSE(stars::ParseCatalogue(bytes).has_value());
	EXPECT_FALSE(stars::ParseCatalogue({}).has_value());
}

TEST(Stars, TheSkyDirectionPointsToTheAxes)
{
	const auto ra0 = stars::SkyDirection(0.0f, 0.0f);
	const auto ra6 = stars::SkyDirection(k_Pi * 0.5f, 0.0f);
	const auto pole = stars::SkyDirection(2.0f, k_Pi * 0.5f);
	EXPECT_NEAR(ra0.x, 1.0f, 1e-6f);
	EXPECT_NEAR(ra6.y, 1.0f, 1e-6f);
	EXPECT_NEAR(pole.z, 1.0f, 1e-6f);
}

TEST(Stars, TheColourIndexGivesTheStarsTemperature)
{
	// The sun (B-V 0.65) is near 5800 K; Vega (0.0) near 10000 K; Betelgeuse (1.85) near 3300 K
	EXPECT_NEAR(stars::TemperatureFromColourIndex(0.65f), 5800.0f, 150.0f);
	EXPECT_NEAR(stars::TemperatureFromColourIndex(0.0f), 10000.0f, 400.0f);
	EXPECT_NEAR(stars::TemperatureFromColourIndex(1.85f), 3300.0f, 200.0f);
	EXPECT_FLOAT_EQ(stars::TemperatureFromColourIndex(-1.0f), stars::TemperatureFromColourIndex(-0.4f));
	EXPECT_LE(stars::TemperatureFromColourIndex(-0.4f), stars::k_HottestKelvin);
	EXPECT_GE(stars::TemperatureFromColourIndex(3.0f), stars::k_CoolestKelvin);
}

TEST(Stars, TheBlackBodyIsRedWhenCoolWhiteNearTheWhitePointAndBlueWhenHot)
{
	const auto cool = stars::BlackbodyColour(3000.0f);
	const auto white = stars::BlackbodyColour(6500.0f);
	const auto hot = stars::BlackbodyColour(20000.0f);
	EXPECT_NEAR(Luminance(cool), 1.0f, 1e-4f);
	EXPECT_NEAR(Luminance(hot), 1.0f, 1e-4f);
	EXPECT_GT(cool.r, cool.g);
	EXPECT_GT(cool.g, cool.b);
	EXPECT_GT(hot.b, hot.g);
	EXPECT_GT(hot.g, hot.r);
	// The white point of sRGB is about 6500 K, so a black body at 6500 K is near grey
	EXPECT_NEAR(white.r / white.b, 1.0f, 0.12f);
	EXPECT_NEAR(white.g / white.b, 1.0f, 0.08f);
	// The blue goes up and the red goes down with the temperature
	float lastBlue = 0.0f;
	for (float kelvin = 3000.0f; kelvin <= 22000.0f; kelvin += 1000.0f)
	{
		const auto colour = stars::BlackbodyColour(kelvin);
		EXPECT_GT(colour.b, lastBlue) << kelvin;
		lastBlue = colour.b;
	}
}

TEST(Stars, TheColourTableGoesFromHotToCool)
{
	EXPECT_EQ(stars::ColourEntry(stars::k_HottestKelvin), 0);
	EXPECT_EQ(stars::ColourEntry(stars::k_CoolestKelvin), stars::k_ColourEntries - 1);
	EXPECT_EQ(stars::ColourEntry(100000.0f), 0);
	EXPECT_LT(stars::ColourEntry(9000.0f), stars::ColourEntry(5000.0f));
}

TEST(Stars, EveryDirectionHasACellAndTheCellsHaveAlmostEqualAreas)
{
	std::vector<int> counts(stars::k_CellCount, 0);
	// Points spread evenly over the sphere (a Fibonacci spiral)
	constexpr int k_Points = 2000000;
	for (int i = 0; i < k_Points; ++i)
	{
		const float z = 1.0f - (2.0f * (static_cast<float>(i) + 0.5f) / static_cast<float>(k_Points));
		const double turns = static_cast<double>(i) * 0.5 * (3.0 - std::sqrt(5.0));
		const auto angle = static_cast<float>(2.0 * std::numbers::pi * (turns - std::floor(turns)));
		const float r = std::sqrt(1.0f - (z * z));
		const auto cell = stars::CellOf({r * std::cos(angle), r * std::sin(angle), z});
		ASSERT_GE(cell.face, 0);
		ASSERT_LT(cell.face, 6);
		ASSERT_GE(cell.column, 0);
		ASSERT_LT(cell.column, stars::k_CellsAcross);
		ASSERT_GE(cell.row, 0);
		ASSERT_LT(cell.row, stars::k_CellsAcross);
		++counts.at(static_cast<size_t>(stars::CellNumber(cell)));
	}
	const auto [fewest, most] = std::ranges::minmax(counts);
	EXPECT_GT(fewest, 0);
	EXPECT_LT(static_cast<float>(most) / static_cast<float>(fewest), 1.6f);
}

TEST(Stars, ARecordKeepsItsColourAndMagnitude)
{
	for (const int entry : {0, 17, 255})
	{
		for (const float magnitude : {-1.46f, 0.0f, 3.21f, 6.5f, 9.49f})
		{
			const auto record = stars::UnpackRecord(stars::PackRecord(entry, magnitude));
			EXPECT_EQ(record.colourEntry, entry);
			EXPECT_NEAR(record.magnitude, magnitude, 0.0051f);
		}
	}
}

TEST(Stars, TheTextureHasTheTableTheIndexAndTheRecords)
{
	const std::vector<stars::CatalogueStar> list {
	    {.rightAscension = 0.3f, .declination = 0.2f, .magnitude = 4.0f, .colourIndex = 1.2f},
	    {.rightAscension = 0.3001f, .declination = 0.2001f, .magnitude = 1.0f, .colourIndex = -0.1f},
	    {.rightAscension = 3.0f, .declination = -0.8f, .magnitude = 2.5f, .colourIndex = {}},
	};
	const auto texture = stars::BuildTexture(list);
	EXPECT_EQ(texture.width, stars::k_TextureWidth);
	EXPECT_EQ(texture.height, 1 + stars::k_IndexRows + 1);
	EXPECT_EQ(texture.texels.size(), static_cast<size_t>(texture.width) * texture.height);
	EXPECT_EQ(texture.mostInCell, 2);
	// The first row is the colour table, hot and blue at its start
	EXPECT_GT(texture.texels.front().b, texture.texels.front().r);
	EXPECT_GT(texture.texels.at(stars::k_ColourEntries - 1).r, texture.texels.at(stars::k_ColourEntries - 1).b);
	// The cell of the two near stars holds both, the brightest first
	const auto direction = stars::SkyDirection(0.3f, 0.2f);
	const auto records = RecordsOf(texture, stars::CellNumber(stars::CellOf(direction)));
	ASSERT_EQ(records.size(), 2u);
	EXPECT_NEAR(stars::UnpackRecord(records[0].w).magnitude, 1.0f, 0.006f);
	EXPECT_NEAR(stars::UnpackRecord(records[1].w).magnitude, 4.0f, 0.006f);
	EXPECT_NEAR(glm::dot(glm::vec3(records[1]), direction), 1.0f, 1e-6f);
	const auto& index =
	    texture.texels.at(static_cast<size_t>(stars::k_TextureWidth + stars::CellNumber(stars::CellOf(direction))));
	EXPECT_NEAR(index.z, 1.0f, 0.006f);
	// A star with no colour index takes the colour of a star like the sun
	const auto third = RecordsOf(texture, stars::CellNumber(stars::CellOf(stars::SkyDirection(3.0f, -0.8f))));
	ASSERT_EQ(third.size(), 1u);
	EXPECT_EQ(stars::UnpackRecord(third[0].w).colourEntry, stars::ColourEntry(stars::TemperatureFromColourIndex(0.65f)));
}

TEST(Stars, AStarNearACellsEdgeIsInTheCellsOnBothSides)
{
	// A direction on the edge between two faces of the cube, moved a little to one side
	const glm::vec3 direction = glm::normalize(glm::vec3(1.0f, 0.4f, 1.0f - 0.001f));
	const float declination = std::asin(direction.z);
	const float rightAscension = std::atan2(direction.y, direction.x);
	const auto texture = stars::BuildTexture(std::vector<stars::CatalogueStar> {
	    {.rightAscension = rightAscension, .declination = declination, .magnitude = 3.0f, .colourIndex = 0.3f}});
	const auto own = stars::CellOf(direction);
	const auto across = stars::CellOf(glm::normalize(glm::vec3(1.0f, 0.4f, 1.0f + 0.001f)));
	ASSERT_NE(own.face, across.face);
	EXPECT_EQ(RecordsOf(texture, stars::CellNumber(own)).size(), 1u);
	EXPECT_EQ(RecordsOf(texture, stars::CellNumber(across)).size(), 1u);
	// A cell far away does not hold it
	EXPECT_EQ(RecordsOf(texture, stars::CellNumber(stars::CellOf(-direction))).size(), 0u);
}

TEST(Stars, TheTwilightHidesTheFaintStarsFirst)
{
	const auto sine = [](float degrees) { return std::sin(degrees * k_Pi / 180.0f); };
	EXPECT_FLOAT_EQ(stars::TwilightDrop(sine(-30.0f)), 0.0f);
	EXPECT_FLOAT_EQ(stars::TwilightDrop(sine(-18.0f)), 0.0f);
	float last = -1.0f;
	for (float degrees = -18.0f; degrees <= 10.0f; degrees += 0.5f)
	{
		const float drop = stars::TwilightDrop(sine(degrees));
		EXPECT_GE(drop, last) << degrees;
		last = drop;
	}
	const stars::SkyLight night {.sunSine = sine(-30.0f), .moonLight = 0.0f, .skyLuminance = 0.0f, .aboveAir = 0.0f};
	EXPECT_FLOAT_EQ(stars::LimitingMagnitude(night), stars::k_DarkLimit);
	// The end of the nautical twilight takes 2 magnitudes, the end of the civil twilight about 5, and the day all stars
	auto dusk = night;
	dusk.sunSine = sine(-12.0f);
	EXPECT_NEAR(stars::LimitingMagnitude(dusk), stars::k_DarkLimit - 2.0f, 0.1f);
	dusk.sunSine = sine(-6.0f);
	EXPECT_NEAR(stars::LimitingMagnitude(dusk), stars::k_DarkLimit - 4.8f, 0.1f);
	dusk.sunSine = sine(5.0f);
	EXPECT_LT(stars::LimitingMagnitude(dusk), -1.5f);
}

TEST(Stars, TheMoonAndABrightSkyHideTheFaintStars)
{
	const stars::SkyLight night {.sunSine = -0.8f, .moonLight = 0.0f, .skyLuminance = stars::k_DarkSky, .aboveAir = 0.0f};
	auto moonlit = night;
	moonlit.moonLight = 1.0f;
	EXPECT_NEAR(stars::LimitingMagnitude(moonlit), stars::k_DarkLimit - stars::k_MoonDrop, 1e-4f);
	moonlit.skyLuminance = 5.0f * stars::k_DarkSky;
	EXPECT_LT(stars::LimitingMagnitude(moonlit), stars::k_DarkLimit - stars::k_MoonDrop);
	// A full moon high in a clear sky gives all its light; a low or covered moon less; a new moon none
	EXPECT_FLOAT_EQ(stars::MoonLight(1.0f, 1.0f, 1.0f), 1.0f);
	EXPECT_LT(stars::MoonLight(1.0f, 0.1f, 1.0f), 0.5f);
	EXPECT_FLOAT_EQ(stars::MoonLight(1.0f, -0.1f, 1.0f), 0.0f);
	EXPECT_FLOAT_EQ(stars::MoonLight(0.0f, 1.0f, 1.0f), 0.0f);
	EXPECT_FLOAT_EQ(stars::MoonLight(1.0f, 1.0f, 0.0f), 0.0f);
}

TEST(Stars, AboveTheAirMoreStarsShowAndNeitherTheDuskNorTheMoonHidesThem)
{
	const stars::SkyLight space {.sunSine = -0.05f, .moonLight = 1.0f, .skyLuminance = 0.0f, .aboveAir = 1.0f};
	EXPECT_FLOAT_EQ(stars::LimitingMagnitude(space), stars::k_DarkLimit + stars::k_SpaceExtra);
}

TEST(Stars, AStarFadesOutBeforeTheFaintestThatShows)
{
	EXPECT_FLOAT_EQ(stars::StarLight(6.0f, 6.0f), 0.0f);
	EXPECT_FLOAT_EQ(stars::StarLight(7.0f, 6.0f), 0.0f);
	EXPECT_NEAR(stars::StarLight(0.0f, 7.0f), stars::k_Brightness, 1e-5f);
	EXPECT_GT(stars::StarLight(-1.46f, 7.0f), stars::StarLight(0.0f, 7.0f));
	EXPECT_GT(stars::StarLight(3.0f, 7.0f), stars::StarLight(5.0f, 7.0f));
	EXPECT_GT(stars::StarLight(5.0f, 7.0f), stars::StarLight(5.0f, 5.5f));
}

TEST(Stars, StarsTwinkleOnlyLowDownAndNotFromAboveTheAir)
{
	EXPECT_NEAR(stars::TwinkleStrength(0.0f, 0.0f), stars::k_TwinkleMost, 1e-6f);
	EXPECT_GT(stars::TwinkleStrength(0.1f, 0.0f), stars::TwinkleStrength(0.3f, 0.0f));
	EXPECT_FLOAT_EQ(stars::TwinkleStrength(std::sin(30.0f * k_Pi / 180.0f), 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(stars::TwinkleStrength(1.0f, 0.0f), 0.0f);
	EXPECT_FLOAT_EQ(stars::TwinkleStrength(0.0f, 1.0f), 0.0f);
	EXPECT_LT(stars::TwinkleStrength(0.05f, 0.5f), stars::TwinkleStrength(0.05f, 0.0f));
}

TEST(Stars, FaintStarsAreManyAndBrightFew)
{
	const auto& layer = stars::k_FaintLayers.front();
	EXPECT_FLOAT_EQ(stars::FaintMagnitude(0.0f, layer.brightest, layer.faintest), layer.brightest);
	EXPECT_NEAR(stars::FaintMagnitude(1.0f, layer.brightest, layer.faintest), layer.faintest, 1e-5f);
	// Over the magnitude range of a layer the count grows about 2.8 times, so more than half are in the faint half
	const float middle = 0.5f * (layer.brightest + layer.faintest);
	EXPECT_LT(stars::FaintMagnitude(0.5f, layer.brightest, layer.faintest), layer.faintest);
	EXPECT_GT(stars::FaintMagnitude(0.5f, layer.brightest, layer.faintest), middle);
}

TEST(Stars, TheFaintLayersHaveAboutTheTrueNumberOfStars)
{
	// The number of stars on the whole sky to a magnitude, near the true counts (about 15000 to 7, 42000 to 8, 120000
	// to 9)
	const auto brighterThan = [](float magnitude) { return 4800.0f * std::pow(10.0f, 0.45f * (magnitude - 6.0f)); };
	for (const auto& layer : stars::k_FaintLayers)
	{
		const float made = 6.0f * layer.cellsAcross * layer.cellsAcross * layer.share;
		const float real = brighterThan(layer.faintest) - brighterThan(layer.brightest);
		EXPECT_NEAR(made / real, 1.0f, 0.35f) << layer.brightest;
	}
}

TEST(Stars, FaintStarsHaveTheUsualShareOfColours)
{
	EXPECT_NEAR(stars::FaintTemperature(0.0f), stars::k_HottestKelvin, 1.0f);
	EXPECT_NEAR(stars::FaintTemperature(1.0f), stars::k_CoolestKelvin, 1.0f);
	float last = stars::k_HottestKelvin + 1.0f;
	int hot = 0;
	int cool = 0;
	for (int i = 0; i <= 100; ++i)
	{
		const float kelvin = stars::FaintTemperature(static_cast<float>(i) / 100.0f);
		EXPECT_LE(kelvin, last);
		last = kelvin;
		hot += kelvin > 10000.0f ? 1 : 0;
		cool += kelvin < 5200.0f ? 1 : 0;
	}
	// Few hot blue stars, more orange giants and red stars
	EXPECT_LT(hot, 12);
	EXPECT_GT(cool, 30);
}
