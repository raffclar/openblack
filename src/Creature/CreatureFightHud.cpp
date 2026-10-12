/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFightHud.h"

#include <cstdint>

#include <algorithm>

using namespace openblack;
using namespace openblack::creature_fight_hud;

namespace
{
/// The unit is a seventieth of the screen's height
constexpr int k_UnitsPerScreen = 70;
/// The shade's size, in units
constexpr int k_BoxWidth = 33;
constexpr int k_BoxHeight = 9;
/// The bars' ends, the names' left edge and their widest, in units
constexpr int k_BarLeft = 1;
constexpr int k_BarRight = 26;
constexpr int k_NameLeft = 27;
constexpr int k_NameWidth = 30;
/// The first row a unit down, the second three and a half units below it
constexpr float k_RowPitch = 3.5f;
/// The health bar is two units high; the stamina bar starts one and a half units down and ends three down
constexpr float k_HealthHeight = 2.0f;
constexpr float k_StaminaTop = 1.5f;
constexpr float k_StaminaBottom = 3.0f;
/// The names are three units high
constexpr int k_TextHeight = 3;

// Every value is held between 0 and 1 first
[[nodiscard]] float Clamp01(float value)
{
	return std::clamp(value, 0.0f, 1.0f);
}
} // namespace

Layout creature_fight_hud::Compute(glm::u16vec2 resolution)
{
	const int unit = resolution.y / k_UnitsPerScreen;
	const auto u = static_cast<float>(unit);
	const auto whole = [u](float units) { return static_cast<float>(static_cast<int>(units * u)); };

	Layout layout;
	layout.unit = unit;
	layout.box = {.min = {0.0f, 0.0f}, .max = {static_cast<float>(k_BoxWidth * unit), static_cast<float>(k_BoxHeight * unit)}};
	layout.textSize = static_cast<float>(k_TextHeight * unit);
	layout.nameWidth = static_cast<float>(k_NameWidth * unit);
	const float left = static_cast<float>(k_BarLeft * unit);
	const float right = static_cast<float>(k_BarRight * unit);
	const float firstTop = u;
	const std::array<float, 2> tops {firstTop, static_cast<float>(static_cast<int>(firstTop + (k_RowPitch * u)))};
	for (size_t i = 0; i < layout.rows.size(); ++i)
	{
		const float top = tops.at(i);
		auto& row = layout.rows.at(i);
		row.name = {static_cast<float>(k_NameLeft * unit), top};
		row.health = {.min = {left, top}, .max = {right, top + whole(k_HealthHeight)}};
		row.stamina = {.min = {left, top + whole(k_StaminaTop)}, .max = {right, top + whole(k_StaminaBottom)}};
	}
	return layout;
}

glm::u8vec3 creature_fight_hud::HealthColour(float health)
{
	const float fill = Clamp01(health);
	if (fill < 0.5f)
	{
		// Red, with green climbing to yellow at half
		const int steps = static_cast<int>(fill * 512.0f);
		return {255, static_cast<uint8_t>((steps * 255) >> 8), 0};
	}
	// Yellow, with red falling away to green when full
	const int steps = static_cast<int>((fill - 0.5f) * -512.0f);
	return {static_cast<uint8_t>(255 + ((255 * steps) >> 8)), 255, 0};
}

glm::u8vec3 creature_fight_hud::StaminaColour(float stamina)
{
	const int steps = static_cast<int>(Clamp01(stamina) * 255.0f);
	return {static_cast<uint8_t>(32 + ((3 * steps) >> 3)), 128, 255};
}

std::u16string_view creature_fight_hud::FitName(std::u16string_view name, float width,
                                                const std::function<float(std::u16string_view)>& widthOf)
{
	while (!name.empty() && widthOf(name) > width)
	{
		name.remove_suffix(1);
	}
	return name;
}
