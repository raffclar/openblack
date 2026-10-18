/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT (not for merge): the rules of the stars of the night sky

#include "Stars.h"

#include <cmath>
#include <cstddef>
#include <cstring>

#include <algorithm>
#include <iterator>
#include <numbers>
#include <utility>

#include <glm/geometric.hpp>

namespace openblack::graphics::stars
{

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
constexpr std::array<char, 4> k_Magic {'O', 'B', 'S', 'C'};
constexpr uint32_t k_Version = 1;
constexpr size_t k_HeaderBytes = 12;
constexpr size_t k_StarBytes = 12;
constexpr int16_t k_NoColour = -32768;
/// The colour of a star that the catalogue gives no colour for: a star like the sun
constexpr float k_UsualColourIndex = 0.65f;
/// A magnitude fainter than every star, for a cell with no star
constexpr float k_NoStar = 99.0f;

template <typename T>
T Read(std::span<const uint8_t> bytes, size_t offset)
{
	T value;
	std::memcpy(&value, bytes.data() + offset, sizeof(T));
	return value;
}

float Smoothstep(float low, float high, float x)
{
	const float t = std::clamp((x - low) / (high - low), 0.0f, 1.0f);
	return t * t * (3.0f - (2.0f * t));
}

/// A lobe of the fit of a colour matching function: a Gaussian with a different width on each side of its peak
float Lobe(float wavelength, float peak, float below, float above)
{
	const float t = (wavelength - peak) / (wavelength < peak ? below : above);
	return std::exp(-0.5f * t * t);
}

/// The CIE 1931 colour matching functions at a wavelength in nanometres (the analytic fit of Wyman, Sloan and Shirley,
/// 2013)
glm::vec3 MatchingFunctions(float wavelength)
{
	const float x = (1.056f * Lobe(wavelength, 599.8f, 37.9f, 31.0f)) + (0.362f * Lobe(wavelength, 442.0f, 16.0f, 26.7f)) -
	                (0.065f * Lobe(wavelength, 501.1f, 20.4f, 26.2f));
	const float y = (0.821f * Lobe(wavelength, 568.8f, 46.9f, 40.5f)) + (0.286f * Lobe(wavelength, 530.9f, 16.3f, 31.1f));
	const float z = (1.217f * Lobe(wavelength, 437.0f, 11.8f, 36.0f)) + (0.681f * Lobe(wavelength, 459.0f, 26.0f, 13.8f));
	return {x, y, z};
}

/// The light of a black body at a wavelength in nanometres, to a constant factor (Planck's law)
double Planck(double wavelength, double kelvin)
{
	constexpr double k_SecondRadiation = 1.438777e7; // nanometre kelvin
	const double scaled = wavelength / 1000.0;
	return 1.0 / (scaled * scaled * scaled * scaled * scaled * (std::exp(k_SecondRadiation / (wavelength * kelvin)) - 1.0));
}

constexpr float Mired(float kelvin)
{
	return 1.0e6f / kelvin;
}
} // namespace

std::optional<std::vector<CatalogueStar>> ParseCatalogue(std::span<const uint8_t> bytes)
{
	if (bytes.size() < k_HeaderBytes || !std::equal(k_Magic.begin(), k_Magic.end(), bytes.begin()) ||
	    Read<uint32_t>(bytes, 4) != k_Version)
	{
		return std::nullopt;
	}
	const auto count = static_cast<size_t>(Read<uint32_t>(bytes, 8));
	if (bytes.size() != k_HeaderBytes + (count * k_StarBytes))
	{
		return std::nullopt;
	}
	std::vector<CatalogueStar> stars;
	stars.reserve(count);
	for (size_t i = 0; i < count; ++i)
	{
		const size_t at = k_HeaderBytes + (i * k_StarBytes);
		const auto colour = Read<int16_t>(bytes, at + 10);
		stars.push_back({
		    .rightAscension = Read<float>(bytes, at),
		    .declination = Read<float>(bytes, at + 4),
		    .magnitude = static_cast<float>(Read<int16_t>(bytes, at + 8)) / 100.0f,
		    .colourIndex = colour == k_NoColour ? std::nullopt : std::optional(static_cast<float>(colour) / 1000.0f),
		});
	}
	return stars;
}

glm::vec3 SkyDirection(float rightAscension, float declination)
{
	return {std::cos(declination) * std::cos(rightAscension), std::cos(declination) * std::sin(rightAscension),
	        std::sin(declination)};
}

float TemperatureFromColourIndex(float colourIndex)
{
	const float bv = std::clamp(colourIndex, -0.4f, 2.0f);
	const float kelvin = 4600.0f * ((1.0f / ((0.92f * bv) + 1.7f)) + (1.0f / ((0.92f * bv) + 0.62f)));
	return std::clamp(kelvin, k_CoolestKelvin, k_HottestKelvin);
}

glm::vec3 BlackbodyColour(float kelvin)
{
	glm::dvec3 xyz {0.0};
	for (int wavelength = 380; wavelength <= 780; wavelength += 5)
	{
		xyz += glm::dvec3(MatchingFunctions(static_cast<float>(wavelength))) *
		       Planck(static_cast<double>(wavelength), static_cast<double>(kelvin));
	}
	xyz /= xyz.y;
	// From XYZ to linear sRGB
	const glm::vec3 rgb {static_cast<float>((3.2406 * xyz.x) - (1.5372 * xyz.y) - (0.4986 * xyz.z)),
	                     static_cast<float>((-0.9689 * xyz.x) + (1.8758 * xyz.y) + (0.0415 * xyz.z)),
	                     static_cast<float>((0.0557 * xyz.x) - (0.2040 * xyz.y) + (1.0570 * xyz.z))};
	const glm::vec3 positive = glm::max(rgb, glm::vec3(0.0f));
	return positive / glm::dot(positive, glm::vec3(0.2126f, 0.7152f, 0.0722f));
}

int ColourEntry(float kelvin)
{
	const float t = (Mired(std::clamp(kelvin, k_CoolestKelvin, k_HottestKelvin)) - Mired(k_HottestKelvin)) /
	                (Mired(k_CoolestKelvin) - Mired(k_HottestKelvin));
	return std::clamp(static_cast<int>(std::lround(t * static_cast<float>(k_ColourEntries - 1))), 0, k_ColourEntries - 1);
}

FacePoint FacePointOf(glm::vec3 direction)
{
	const glm::vec3 a = glm::abs(direction);
	if (a.x >= a.y && a.x >= a.z)
	{
		return {.face = direction.x > 0.0f ? 0 : 1, .across = direction.y / a.x, .up = direction.z / a.x};
	}
	if (a.y >= a.z)
	{
		return {.face = direction.y > 0.0f ? 2 : 3, .across = direction.z / a.y, .up = direction.x / a.y};
	}
	return {.face = direction.z > 0.0f ? 4 : 5, .across = direction.x / a.z, .up = direction.y / a.z};
}

float FaceWarp(float coordinate)
{
	return (4.0f / k_Pi) * std::atan(coordinate);
}

Cell CellOf(glm::vec3 direction)
{
	const auto point = FacePointOf(direction);
	const auto index = [](float coordinate) {
		const float t = (FaceWarp(coordinate) + 1.0f) * 0.5f * static_cast<float>(k_CellsAcross);
		return std::clamp(static_cast<int>(std::floor(t)), 0, k_CellsAcross - 1);
	};
	return {.face = point.face, .column = index(point.across), .row = index(point.up)};
}

int CellNumber(const Cell& cell)
{
	return (cell.face * k_CellsAcross * k_CellsAcross) + (cell.row * k_CellsAcross) + cell.column;
}

float PackRecord(int colourEntry, float magnitude)
{
	const float steps = std::clamp(std::round((magnitude - k_LowestMagnitude) * 100.0f), 0.0f, k_ColourStride - 1.0f);
	return (static_cast<float>(colourEntry) * k_ColourStride) + steps;
}

Record UnpackRecord(float packed)
{
	const float entry = std::floor(packed / k_ColourStride);
	return {.colourEntry = static_cast<int>(entry),
	        .magnitude = ((packed - (entry * k_ColourStride)) / 100.0f) + k_LowestMagnitude};
}

StarTexture BuildTexture(std::span<const CatalogueStar> stars)
{
	// The records of each cell, with their magnitudes for the order
	std::vector<std::vector<std::pair<float, glm::vec4>>> cells(k_CellCount);
	constexpr int k_Around = 8;
	for (const auto& star : stars)
	{
		const glm::vec3 direction = SkyDirection(star.rightAscension, star.declination);
		const float kelvin = TemperatureFromColourIndex(star.colourIndex.value_or(k_UsualColourIndex));
		const glm::vec4 record {direction, PackRecord(ColourEntry(kelvin), star.magnitude)};
		// The cell of the star and the cells of the points round it at the margin's angle
		const glm::vec3 side = std::abs(direction.z) < 0.9f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
		const glm::vec3 first = glm::normalize(glm::cross(direction, side));
		const glm::vec3 second = glm::cross(direction, first);
		std::array<int, k_Around + 1> numbers {};
		numbers.at(0) = CellNumber(CellOf(direction));
		for (int i = 0; i < k_Around; ++i)
		{
			const float angle = 2.0f * k_Pi * static_cast<float>(i) / static_cast<float>(k_Around);
			const glm::vec3 round = (first * std::cos(angle)) + (second * std::sin(angle));
			const glm::vec3 point = (direction * std::cos(k_CellMargin)) + (round * std::sin(k_CellMargin));
			numbers.at(static_cast<size_t>(i) + 1) = CellNumber(CellOf(glm::normalize(point)));
		}
		std::ranges::sort(numbers);
		const auto [last, end] = std::ranges::unique(numbers);
		for (auto it = numbers.begin(); it != last; ++it)
		{
			cells.at(static_cast<size_t>(*it)).emplace_back(star.magnitude, record);
		}
	}

	std::vector<glm::vec4> records;
	std::vector<glm::vec4> index(k_CellCount);
	int mostInCell = 0;
	for (size_t i = 0; i < cells.size(); ++i)
	{
		auto& cell = cells[i];
		std::ranges::stable_sort(cell, {}, &std::pair<float, glm::vec4>::first);
		index[i] = {static_cast<float>(records.size()), static_cast<float>(cell.size()),
		            cell.empty() ? k_NoStar : cell.front().first, 0.0f};
		mostInCell = std::max(mostInCell, static_cast<int>(cell.size()));
		std::ranges::transform(cell, std::back_inserter(records), &std::pair<float, glm::vec4>::second);
	}

	const size_t recordRows = (records.size() + k_TextureWidth - 1) / k_TextureWidth;
	StarTexture texture {.width = k_TextureWidth,
	                     .height = static_cast<uint16_t>(1 + k_IndexRows + recordRows),
	                     .texels = {},
	                     .mostInCell = mostInCell};
	texture.texels.resize(static_cast<size_t>(texture.width) * texture.height, glm::vec4(0.0f));
	for (int i = 0; i < k_ColourEntries; ++i)
	{
		const float mired = Mired(k_HottestKelvin) + ((Mired(k_CoolestKelvin) - Mired(k_HottestKelvin)) *
		                                              static_cast<float>(i) / static_cast<float>(k_ColourEntries - 1));
		texture.texels[static_cast<size_t>(i)] = glm::vec4(BlackbodyColour(1.0e6f / mired), 1.0f);
	}
	std::ranges::copy(index, texture.texels.begin() + k_TextureWidth);
	std::ranges::copy(records, texture.texels.begin() + (static_cast<ptrdiff_t>(1 + k_IndexRows) * k_TextureWidth));
	return texture;
}

float TwilightDrop(float sunSine)
{
	// The sun's height in degrees and the drop of the faintest star that shows
	constexpr std::array<std::pair<float, float>, 8> k_Drops {{
	    {-18.0f, 0.0f},
	    {-15.0f, 0.8f},
	    {-12.0f, 2.0f},
	    {-9.0f, 3.2f},
	    {-6.0f, 4.8f},
	    {-3.0f, 6.5f},
	    {0.0f, 8.5f},
	    {4.0f, 12.0f},
	}};
	const float degrees = std::asin(std::clamp(sunSine, -1.0f, 1.0f)) * 180.0f / k_Pi;
	if (degrees <= k_Drops.front().first)
	{
		return k_Drops.front().second;
	}
	for (size_t i = 1; i < k_Drops.size(); ++i)
	{
		const auto [height, drop] = k_Drops.at(i);
		if (degrees <= height)
		{
			const auto [lastHeight, lastDrop] = k_Drops.at(i - 1);
			return lastDrop + ((drop - lastDrop) * (degrees - lastHeight) / (height - lastHeight));
		}
	}
	return k_Drops.back().second;
}

float LimitingMagnitude(const SkyLight& sky)
{
	const float aboveAir = std::clamp(sky.aboveAir, 0.0f, 1.0f);
	const float inAir = 1.0f - aboveAir;
	// The brightness of the sky as a number of moonless night skies
	const float twilight = std::pow(10.0f, inAir * TwilightDrop(sky.sunSine) / k_DropScale) - 1.0f;
	const float moon = std::pow(10.0f, inAir * k_MoonDrop * std::clamp(sky.moonLight, 0.0f, 1.0f) / k_DropScale) - 1.0f;
	const float screen = std::max((sky.skyLuminance / k_DarkSky) - 1.0f, 0.0f);
	const float brightness = 1.0f + twilight + moon + screen;
	return k_DarkLimit + (aboveAir * k_SpaceExtra) - (k_DropScale * std::log10(brightness));
}

float MoonLight(float phaseBrightness, float elevationSine, float clearSky)
{
	return std::clamp(phaseBrightness, 0.0f, 1.0f) * Smoothstep(-0.02f, 0.5f, elevationSine) * std::clamp(clearSky, 0.0f, 1.0f);
}

float StarLight(float magnitude, float limitingMagnitude)
{
	return k_Brightness * std::pow(10.0f, -0.4f * k_EyeResponse * magnitude) *
	       Smoothstep(0.0f, k_FadeMagnitudes, limitingMagnitude - magnitude);
}

float TwinkleStrength(float elevationSine, float aboveAir)
{
	const float low = 1.0f - Smoothstep(0.0f, k_TwinkleTop, elevationSine);
	return k_TwinkleMost * low * low * (1.0f - std::clamp(aboveAir, 0.0f, 1.0f));
}

float FaintMagnitude(float u, float brightest, float faintest)
{
	const float a = k_CountSlope * std::numbers::ln10_v<float>;
	const float span = std::expm1(a * (faintest - brightest));
	return brightest + (std::log1p(std::clamp(u, 0.0f, 1.0f) * span) / a);
}

float FaintTemperature(float u)
{
	// The share of the stars up to each temperature, from the hottest
	constexpr std::array<std::pair<float, float>, 7> k_Shares {{
	    {0.0f, 22000.0f},
	    {0.08f, 10000.0f},
	    {0.30f, 7500.0f},
	    {0.48f, 6000.0f},
	    {0.63f, 5200.0f},
	    {0.94f, 3900.0f},
	    {1.0f, 3000.0f},
	}};
	const float t = std::clamp(u, 0.0f, 1.0f);
	for (size_t i = 1; i < k_Shares.size(); ++i)
	{
		const auto [share, kelvin] = k_Shares.at(i);
		if (t <= share)
		{
			const auto [lastShare, lastKelvin] = k_Shares.at(i - 1);
			const float along = (t - lastShare) / (share - lastShare);
			return 1.0e6f / (Mired(lastKelvin) + ((Mired(kelvin) - Mired(lastKelvin)) * along));
		}
	}
	return k_CoolestKelvin;
}

} // namespace openblack::graphics::stars
