/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec4.hpp>

#include "ECS/Components/GameStanding.h"
#include "Enums.h"

/// The rules of the box of the winning conditions of a network game, which the Multiplayer tab opens. Free of the
/// game's state so they can be tested on their own.
///
/// The box shows a line graph of the score history of each god among the first four players, and a list of the
/// winning conditions of the player of this computer. Each line of the list shows the name and the target of a
/// condition, and in the right third of the line how far each god got: its value, or "Done" when it completed the
/// condition. A check box for each god shows or hides the god in the graph and in the list.
namespace openblack::gui::conditions_box
{

/// The number of the check boxes: one for each of the first four players
inline constexpr size_t k_Gods = 4;

/// A god in the box
struct God
{
	PlayerNames player {PlayerNames::PLAYER_ONE};
	glm::vec4 colour {1.0f};
	/// The values of the score history that the game shows
	std::vector<float> scores;
	std::array<std::optional<ecs::components::WinCondition>, ecs::components::GameStanding::k_ConditionCount> conditions {};
};

/// What the box shows as it opens
struct Figures
{
	/// The gods that a human or a computer mind plays among the first four players, in the order of the players
	std::vector<God> gods;
	/// The winning conditions of the player of this computer, in the order of their numbers: the number and the target
	struct Condition
	{
		size_t number {0};
		int32_t target {0};
	};
	std::vector<Condition> conditions;
};

/// The name of the text of a winning condition in the multiplayer texts. The texts skip the number 13.
[[nodiscard]] std::string_view ConditionTextName(size_t number);

/// The text of a line of the list: the name of the condition and, in brackets, its target
[[nodiscard]] std::u16string LineText(std::u16string_view name, int32_t target);

/// The text of how far a god got with a condition: the value, never more than the target, or the text for "Done"
[[nodiscard]] std::u16string ValueText(const ecs::components::WinCondition& condition, std::u16string_view done);

/// Where the values of a line go: the right third of the line, one column for each god shown. Each value is centred on
/// the left edge of its column and cut short to the width of the column.
struct Columns
{
	int first {0};
	int width {0};
};
[[nodiscard]] Columns ValueColumns(int left, int right, size_t gods);

/// The gods that the box shows: the gods whose check box is ticked
[[nodiscard]] std::vector<size_t> ShownGods(const Figures& figures, const std::array<bool, k_Gods>& ticked);

} // namespace openblack::gui::conditions_box
