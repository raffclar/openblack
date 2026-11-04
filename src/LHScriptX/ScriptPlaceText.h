/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdlib>

#include <optional>
#include <string>

#include <glm/vec2.hpp>

#include "3D/MapCoords.h"

namespace openblack::lhscriptx
{

/// A place that a land script gives as text: "x,z", or "x,z,height"
struct ScriptPlaceText
{
	/// The place in metres along the two map axes
	glm::vec2 metres {0.0f};
	/// The exact map position that the game reads from the text. Its altitude is the height above the land.
	map_coords::MapCoords onMap {};
};

/// Reads a place of a land script. The text has two numbers, or three or more numbers, with a comma between each two.
/// The game reads an optional third number as the height of the place above the land, rounded to a float, and reads
/// no number after it. An empty number is 0, as in the game. A text in a different form is not a place.
[[nodiscard]] inline std::optional<ScriptPlaceText> ReadScriptPlace(const std::string& text)
{
	const char* const start = text.c_str();
	const char* const end = start + text.size();
	char* after = nullptr;
	const float x = std::strtof(start, &after);
	if (*after != ',')
	{
		return std::nullopt;
	}
	const char* const zStart = after + 1;
	const float z = std::strtof(zStart, &after);
	if (after != end && *after != ',')
	{
		return std::nullopt;
	}
	float altitude = 0.0f;
	if (after != end)
	{
		const char* const heightStart = after + 1;
		const double height = std::strtod(heightStart, &after);
		// Numbers after the third are not read, as in the game
		if (after != end && *after != ',')
		{
			return std::nullopt;
		}
		altitude = static_cast<float>(height);
	}
	auto onMap = map_coords::ScriptPlace(std::strtod(start, nullptr), std::strtod(zStart, nullptr));
	onMap.altitude = altitude;
	return ScriptPlaceText {.metres = {x, z}, .onMap = onMap};
}

} // namespace openblack::lhscriptx
