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
#include <span>
#include <string_view>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

/// EXPERIMENT (not for merge): the stars of the night sky of the water experiment. The sky's shader (stars.sh) uses
/// the same rules as this file; the tests check these rules.
/// - The bright stars come from the Yale Bright Star Catalogue (5th revised edition, CDS V/50, NASA public-domain
///   data), at their true places on the sky's sphere (celestial.sh), with their true brightness and colour.
/// - Layers of fainter stars that the shader makes from a hash fill the sky between them.
/// - The light of the sky behind a star sets the faintest star that the eye sees, so the moon and the dusk hide the
///   faint stars.
/// tools/experiment/make_star_catalogue.py makes the catalogue file (k_CataloguePath) from the catalogue's text.
namespace openblack::graphics::stars
{

/// The catalogue file and its texture in the texture cache
inline constexpr std::string_view k_CataloguePath = "E:/openblack/experiments/underwater/stars_bsc5.bin";
inline constexpr entt::hashed_string k_TextureId = entt::hashed_string("experiment/star_catalogue");

/// A star of the catalogue
struct CatalogueStar
{
	/// The right ascension and the declination in radians
	float rightAscension;
	float declination;
	/// The visual magnitude: a smaller number is a brighter star
	float magnitude;
	/// The B-V colour index, when the catalogue gives it
	std::optional<float> colourIndex;
};

/// The stars of a catalogue file, or nothing when the file is not a catalogue file
[[nodiscard]] std::optional<std::vector<CatalogueStar>> ParseCatalogue(std::span<const uint8_t> bytes);

/// The direction of a point of the sky's sphere: x towards a right ascension of 0, y towards 6 hours, z to the north pole
[[nodiscard]] glm::vec3 SkyDirection(float rightAscension, float declination);

/// The coolest and the hottest star colour
inline constexpr float k_CoolestKelvin = 3000.0f;
inline constexpr float k_HottestKelvin = 22000.0f;

/// The temperature of a star from its B-V colour index (the formula of Ballesteros, 2012), in the range of the colours
[[nodiscard]] float TemperatureFromColourIndex(float colourIndex);

/// The colour of a black body at a temperature in linear sRGB, with a luminance of 1. Planck's law gives the light of
/// each wavelength; an analytic fit of the CIE 1931 colour matching functions turns it into a colour. The white point is
/// that of sRGB (6500 K).
[[nodiscard]] glm::vec3 BlackbodyColour(float kelvin);

/// The texture of the stars has 256 texels in each row. Its first row is a table of colours from the hottest to the
/// coolest star, in equal steps of the inverse of the temperature.
inline constexpr uint16_t k_TextureWidth = 256;
inline constexpr int k_ColourEntries = k_TextureWidth;
/// The entry of the colour table for a temperature
[[nodiscard]] int ColourEntry(float kelvin);

/// The sky's sphere is divided into the cells of a cube: 6 faces of k_CellsAcross by k_CellsAcross cells. The faces
/// are warped so that their cells have almost equal areas.
inline constexpr int k_CellsAcross = 32;
inline constexpr int k_CellCount = 6 * k_CellsAcross * k_CellsAcross;
/// The rows of the cells' index after the colour table: each texel holds the first record of a cell, the number of its
/// records and the magnitude of its brightest star
inline constexpr int k_IndexRows = k_CellCount / k_TextureWidth;
/// A star goes into each cell that is nearer than this angle in radians, so a pixel near a cell's edge finds it
inline constexpr float k_CellMargin = 0.004f;

/// A cell of the cube: its face (0 +x, 1 -x, 2 +y, 3 -y, 4 +z, 5 -z) and its column and row on the face
struct Cell
{
	int face;
	int column;
	int row;
};
/// The point of a face (-1 to 1 across and up) in a direction, before the warp
struct FacePoint
{
	int face;
	float across;
	float up;
};
[[nodiscard]] FacePoint FacePointOf(glm::vec3 direction);
/// The warp of a face's coordinate that gives the cells almost equal areas
[[nodiscard]] float FaceWarp(float coordinate);
/// The cell in a direction
[[nodiscard]] Cell CellOf(glm::vec3 direction);
/// The number of a cell in the index
[[nodiscard]] int CellNumber(const Cell& cell);

/// The magnitude in a record is kept to 0.01 in steps from k_LowestMagnitude, with the colour entry over it
inline constexpr float k_LowestMagnitude = -2.0f;
inline constexpr float k_ColourStride = 2048.0f;
/// The w of a record: the colour entry and the magnitude in one float, which keeps them exactly
[[nodiscard]] float PackRecord(int colourEntry, float magnitude);
struct Record
{
	int colourEntry;
	float magnitude;
};
[[nodiscard]] Record UnpackRecord(float packed);

/// The texture of the stars: the colour table, the index of the cells and the records of the stars, as floats. Each
/// record is the star's direction on the sky's sphere and its packed colour and magnitude. A cell's records go from
/// the brightest star to the faintest.
struct StarTexture
{
	uint16_t width;
	uint16_t height;
	std::vector<glm::vec4> texels;
	/// The most records in one cell
	int mostInCell;
};
[[nodiscard]] StarTexture BuildTexture(std::span<const CatalogueStar> stars);

/// The faintest star that the eye sees on a dark moonless night from the ground, and how many magnitudes fainter it
/// sees from above the air
inline constexpr float k_DarkLimit = 7.5f;
inline constexpr float k_SpaceExtra = 1.5f;

/// How many magnitudes the twilight takes from the faintest star that shows, from the sine of the sun's height over
/// the horizon: none when the sun is 18 degrees down, about 2 at the end of the nautical twilight, about 5 at the end
/// of the civil twilight, and all stars go by day
[[nodiscard]] float TwilightDrop(float sunSine);

/// The magnitudes that the light of a full moon high in the sky takes from the faintest star that shows
inline constexpr float k_MoonDrop = 2.2f;
/// The luminance of the moonless night sky on the screen. A brighter sky on the screen, as near the moon or in the
/// aurora, hides more stars.
inline constexpr float k_DarkSky = 0.012f;
/// Each light of the sky adds to the sky's brightness. The faintest star that shows is this many magnitudes brighter
/// for each step of ten in the sky's brightness.
inline constexpr float k_DropScale = 2.0f;

/// What lights the sky behind a star
struct SkyLight
{
	/// The sine of the sun's height over the horizon
	float sunSine;
	/// The moon's light, from 0 (no moon) to 1 (a full moon high in a clear sky)
	float moonLight;
	/// The luminance of the sky behind the star on the screen
	float skyLuminance;
	/// 0 in the air to 1 above the air
	float aboveAir;
};

/// The faintest star that shows against a sky. The twilight, the moon and the bright parts of the sky add their light,
/// so the eye sees fewer stars. Above the air the sky is black, so more stars show.
[[nodiscard]] float LimitingMagnitude(const SkyLight& sky);

/// The moon's light for the stars, from its phase's brightness (0 new to 1 full), the sine of its height over the
/// horizon and how clear the sky is
[[nodiscard]] float MoonLight(float phaseBrightness, float elevationSine, float clearSky);

/// The light of a star on the screen, all its pixels together, as the eye sees it: a star of magnitude 0 gives
/// k_Brightness. Stars fade out over k_FadeMagnitudes before the faintest that shows.
inline constexpr float k_Brightness = 5.5f;
inline constexpr float k_EyeResponse = 0.55f;
inline constexpr float k_FadeMagnitudes = 1.5f;
[[nodiscard]] float StarLight(float magnitude, float limitingMagnitude);

/// How much a star twinkles, from the sine of its height over the horizon and `aboveAir`: most at the horizon, none
/// from the height of k_TwinkleTop up and none from above the air
inline constexpr float k_TwinkleMost = 0.55f;
inline constexpr float k_TwinkleTop = 0.42f;
[[nodiscard]] float TwinkleStrength(float elevationSine, float aboveAir);

/// A layer of faint stars that the shader makes: the cells across a face of its cube, the share of the cells with a
/// star and the range of the magnitudes
struct FaintLayer
{
	float cellsAcross;
	float share;
	float brightest;
	float faintest;
};
inline constexpr std::array<FaintLayer, 3> k_FaintLayers {{
    {.cellsAcross = 72.0f, .share = 0.6f, .brightest = 6.5f, .faintest = 7.5f},
    {.cellsAcross = 112.0f, .share = 0.6f, .brightest = 7.5f, .faintest = 8.5f},
    {.cellsAcross = 175.0f, .share = 0.6f, .brightest = 8.5f, .faintest = 9.5f},
}};
/// How fast the number of stars grows with the magnitude: about 2.8 times more for each magnitude
inline constexpr float k_CountSlope = 0.45f;
/// The magnitude of a faint star from a hash `u` from 0 to 1, so that the faint stars are many and the bright few
[[nodiscard]] float FaintMagnitude(float u, float brightest, float faintest);
/// The temperature of a faint star from a hash `u` from 0 to 1: a share of hot blue, white, yellow, orange and red
/// stars like the stars that the eye sees
[[nodiscard]] float FaintTemperature(float u);

} // namespace openblack::graphics::stars
