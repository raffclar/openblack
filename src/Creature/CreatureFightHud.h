/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <functional>
#include <string>
#include <string_view>

#include <glm/gtc/type_precision.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Enums.h"

/// The panel shown during a fight, flush in the screen's top left corner: a shade that fades out from that corner, and
/// a row for each fighter with its fight health bar over its stamina bar and its name to their right. Everything is
/// measured in a unit of a seventieth of the screen's height, whole pixels, so the panel is the same share of every
/// screen's height and keeps to the left edge on wide screens. There are no numbers, and it never fades.
namespace openblack::creature_fight_hud
{
struct Values
{
	struct Side
	{
		std::u16string name;
		float health {1.0f};
		float stamina {1.0f};
	};
	std::array<Side, 2> sides;
};

struct Rect
{
	glm::vec2 min {0.0f};
	glm::vec2 max {0.0f};
};

struct Layout
{
	/// The unit everything is measured in: a seventieth of the screen's height, in whole pixels
	int unit {0};
	/// The shade, darkest at its top left corner
	Rect box;
	float textSize {0.0f};
	/// How wide a name may be before it is cut short
	float nameWidth {0.0f};
	struct Row
	{
		/// The name's top left
		glm::vec2 name;
		/// The health bar, drawn over the lower stamina bar, which it overlaps
		Rect health;
		Rect stamina;
	};
	std::array<Row, 2> rows;
};

/// Where everything goes on a screen of a resolution, in pixels
[[nodiscard]] Layout Compute(glm::u16vec2 resolution);
/// The health bar's colour, as 0-255 channels: red when empty, yellow at half, green when full, in whole steps
[[nodiscard]] glm::u8vec3 HealthColour(float health);
/// The stamina bar's colour, as 0-255 channels: blue, a little paler the more stamina there is
[[nodiscard]] glm::u8vec3 StaminaColour(float stamina);
/// The longest start of a name that fits a width, letters taken off its end one at a time
[[nodiscard]] std::u16string_view FitName(std::u16string_view name, float width,
                                          const std::function<float(std::u16string_view)>& widthOf);
} // namespace openblack::creature_fight_hud
