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

#include <functional>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>

/// The editor's brush for the land: it raises or lowers the corners of the land's cells in a circle, or paints their
/// country, as plain rules with no state of the game
namespace openblack::editor::land_brush
{

enum class Mode : uint8_t
{
	Raise,
	Lower,
	Paint,
};

/// The settings of the brush. In the inner circle a corner takes the full change. Between the inner and the outer
/// circle the change becomes less, to none at the outer circle. A painted corner takes the country in all of the outer
/// circle.
struct Brush
{
	Mode mode {Mode::Raise};
	float innerRadius {10.0f};
	float outerRadius {30.0f};
	/// The altitude steps that a corner in the inner circle goes up or down for each stroke
	int step {4};
	/// The country that a paint gives, an index into the land's countries
	uint8_t country {0};
};

/// The distance between two corners of the land's cells, in metres
constexpr float k_CornerSpacing = 10.0f;
/// The count of corners along each side of the map
constexpr int k_MapCorners = 512;
/// The countries that a cell can have: the cell keeps its country in four bits
constexpr int k_MostCountries = 16;
/// The largest radius of the brush, in metres
constexpr float k_MostRadius = 400.0f;

struct AltitudeChange
{
	glm::u16vec2 corner {0, 0};
	uint8_t altitude {0};

	friend bool operator==(const AltitudeChange&, const AltitudeChange&) = default;
};

struct CountryChange
{
	glm::u16vec2 corner {0, 0};
	uint8_t country {0};

	friend bool operator==(const CountryChange&, const CountryChange&) = default;
};

/// The part of the full change that a corner at a distance from the middle of the brush takes: 1 in the inner circle,
/// less towards the outer circle, 0 at the outer circle and out of it
[[nodiscard]] float Weight(float distance, float innerRadius, float outerRadius);

/// An altitude after a stroke that takes a part of the step: raised (or lowered, for a negative step) by the step times
/// the part, to the nearest whole step, and kept between 0 and 255
[[nodiscard]] uint8_t Brushed(uint8_t altitude, int step, float weight);

/// The corners of the map at or in the outer circle round a point in metres, row by row along x
[[nodiscard]] std::vector<glm::u16vec2> CornersInReach(glm::vec2 centre, float outerRadius);

/// The altitude of a corner, none where the map has no land
using AltitudeAt = std::function<std::optional<uint8_t>(glm::u16vec2)>;
/// The country of a corner, none where the map has no land
using CountryAt = std::function<std::optional<uint8_t>(glm::u16vec2)>;

/// The corners that a stroke of the brush raises or lowers at a point in metres, with their new altitudes. Corners with
/// no land and corners that do not change are not in the list. A paint brush changes no altitude.
[[nodiscard]] std::vector<AltitudeChange> Stroke(const Brush& brush, glm::vec2 centre, const AltitudeAt& altitudeAt);

/// The corners that a paint of the brush gives its country at a point in metres. Corners with no land and corners
/// that already have the country are not in the list. A country past the countries of the land paints nothing.
[[nodiscard]] std::vector<CountryChange> Paint(const Brush& brush, glm::vec2 centre, int countries, const CountryAt& countryAt);

/// Whether a drag of the brush moved far enough from the point of the last stroke to make the next stroke: one
/// corner spacing
[[nodiscard]] bool MovedFarEnough(glm::vec2 lastStroke, glm::vec2 now);

} // namespace openblack::editor::land_brush
