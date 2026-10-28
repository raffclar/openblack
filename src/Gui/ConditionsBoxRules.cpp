/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ConditionsBoxRules.h"

#include <algorithm>

namespace openblack::gui::conditions_box
{

namespace
{
std::u16string Unsigned(uint32_t value)
{
	const auto digits = std::to_string(value);
	return {digits.begin(), digits.end()};
}
} // namespace

std::string_view ConditionTextName(size_t number)
{
	static constexpr std::array<std::string_view, ecs::components::GameStanding::k_ConditionCount> k_Names {
	    "HELP_TEXT_MULTIPLAYER_01", "HELP_TEXT_MULTIPLAYER_02", "HELP_TEXT_MULTIPLAYER_03", "HELP_TEXT_MULTIPLAYER_04",
	    "HELP_TEXT_MULTIPLAYER_05", "HELP_TEXT_MULTIPLAYER_06", "HELP_TEXT_MULTIPLAYER_07", "HELP_TEXT_MULTIPLAYER_08",
	    "HELP_TEXT_MULTIPLAYER_09", "HELP_TEXT_MULTIPLAYER_10", "HELP_TEXT_MULTIPLAYER_11", "HELP_TEXT_MULTIPLAYER_12",
	    "HELP_TEXT_MULTIPLAYER_14", "HELP_TEXT_MULTIPLAYER_15", "HELP_TEXT_MULTIPLAYER_16",
	};
	return number < k_Names.size() ? k_Names.at(number) : std::string_view {};
}

std::u16string LineText(std::u16string_view name, int32_t target)
{
	// The game writes the target as a number with no sign
	return std::u16string(name) + u" (" + Unsigned(static_cast<uint32_t>(target)) + u")";
}

std::u16string ValueText(const ecs::components::WinCondition& condition, std::u16string_view done)
{
	if (condition.completed)
	{
		return std::u16string(done);
	}
	// The game compares the value and the target as numbers with no sign
	return Unsigned(std::min(static_cast<uint32_t>(condition.current), static_cast<uint32_t>(condition.target)));
}

Columns ValueColumns(int left, int right, size_t gods)
{
	if (gods == 0)
	{
		return {};
	}
	// A third of the line, cut towards nothing, then a share of it for each god, with no sign
	const int third = (right - left) / 3;
	const auto width = static_cast<int>(static_cast<uint32_t>(third) / static_cast<uint32_t>(gods));
	return {.first = left + (third * 2), .width = width};
}

std::vector<size_t> ShownGods(const Figures& figures, const std::array<bool, k_Gods>& ticked)
{
	std::vector<size_t> shown;
	for (size_t i = 0; i < figures.gods.size(); ++i)
	{
		const auto number = static_cast<size_t>(figures.gods[i].player);
		if (number < k_Gods && ticked.at(number))
		{
			shown.push_back(i);
		}
	}
	return shown;
}

} // namespace openblack::gui::conditions_box
