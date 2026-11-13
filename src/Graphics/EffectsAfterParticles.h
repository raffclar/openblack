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

#include "WorldLabelGroup.h"

/// The things that the game adds to its sort of the things that blend after the particles, in the game's order. At an
/// equal distance the sort keeps the order in which the things come, so the renderer submits them in this order.
namespace openblack::graphics::effects_after_particles
{

enum class Step : uint8_t
{
	/// The domes of the shields that the spells cast
	ShieldDomes,
	/// The column over each village centre, and just after it the symbols of belief of that centre
	TownColumns,
	/// The numbers that float up from the things that change
	FloatingNumbers,
	/// The rain about the camera
	Rain,
	/// The snow that falls about the camera
	Snowfall,
	/// The names over the villagers
	VillagerNames,
	/// The bubble of the advisors' tips
	TipBubble,
};

inline constexpr std::array k_Order {
    Step::ShieldDomes, Step::TownColumns,   Step::FloatingNumbers, Step::Rain,
    Step::Snowfall,    Step::VillagerNames, Step::TipBubble,
};

/// The group of labels in the world that a step submits, none for a step that is not a group of labels
[[nodiscard]] constexpr std::optional<WorldLabelGroup> LabelsOf(Step step) noexcept
{
	switch (step)
	{
	case Step::FloatingNumbers:
		return WorldLabelGroup::FloatingNumbers;
	case Step::VillagerNames:
		return WorldLabelGroup::VillagerNames;
	case Step::TipBubble:
		return WorldLabelGroup::TipBubble;
	default:
		return std::nullopt;
	}
}

} // namespace openblack::graphics::effects_after_particles
