/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <algorithm>

/// The heights of a land in height units of 0.67 m. A cell of a landscape keeps its altitude in one byte, so the land of
/// a story stops at 255 units (170.85 m). A generated land keeps the height of each corner in more units, so that it can
/// have high mountains. Its altitude byte stays for the countries and the rules of the game. The curve gives the height
/// units of each altitude: below the knee, one unit for each altitude unit, as on the lands of the story; above the
/// knee, more and more units, so that the highest altitude is at the top height.
namespace openblack::landgen
{
/// The metres of one height unit, the same as one altitude unit on the lands of the story
constexpr float k_MetresPerUnit = 0.67f;
/// The highest altitude that a cell can keep
constexpr float k_TopAltitude = 255.0f;
/// The most height units that a corner of a generated land can have (2743 m)
constexpr float k_MostUnits = 4095.0f;

struct HeightCurve
{
	/// Above this altitude, each altitude unit adds more height units
	float knee {k_TopAltitude};
	/// The extra height units for the square of the altitude units above the knee. 0 gives the lands of the story.
	float bend {0.0f};

	/// The curve that puts the highest altitude at the top height in metres. A top height that is not above the height of
	/// the lands of the story, or a knee that is not below the highest altitude, gives the curve of the lands of the
	/// story. The top height stops at the most height units.
	[[nodiscard]] static HeightCurve WithTop(float knee, float topMetres)
	{
		const float topUnits = std::min(topMetres / k_MetresPerUnit, k_MostUnits);
		if (knee >= k_TopAltitude || topUnits <= k_TopAltitude)
		{
			return {};
		}
		const float above = k_TopAltitude - knee;
		return {.knee = knee, .bend = (topUnits - k_TopAltitude) / (above * above)};
	}

	/// True for the curve of the lands of the story: one height unit for each altitude unit
	[[nodiscard]] bool IsLinear() const { return bend == 0.0f; }

	/// The height units of an altitude, not rounded. Below the knee, the altitude itself.
	[[nodiscard]] float UnitsOf(float altitude) const
	{
		if (bend == 0.0f || altitude <= knee)
		{
			return altitude;
		}
		const float above = altitude - knee;
		return altitude + (bend * above * above);
	}

	/// The height units of an altitude, rounded to a whole unit
	[[nodiscard]] uint16_t WholeUnitsOf(float altitude) const
	{
		return static_cast<uint16_t>(std::clamp(std::floor(UnitsOf(altitude) + 0.5f), 0.0f, k_MostUnits));
	}

	/// The height in metres of an altitude
	[[nodiscard]] float MetresOf(float altitude) const { return UnitsOf(altitude) * k_MetresPerUnit; }

	/// The altitude of a height in units: the inverse of UnitsOf, not rounded
	[[nodiscard]] float AltitudeOf(float units) const
	{
		if (bend == 0.0f || units <= knee)
		{
			return units;
		}
		// The root of bend u^2 + u + knee - units = 0, where u is the altitude above the knee
		const float root = std::sqrt(1.0f + (4.0f * bend * (units - knee)));
		return knee + ((root - 1.0f) / (2.0f * bend));
	}

	/// The height in metres of the highest altitude
	[[nodiscard]] float HighestMetres() const { return MetresOf(k_TopAltitude); }

	bool operator==(const HeightCurve&) const = default;
};
} // namespace openblack::landgen
