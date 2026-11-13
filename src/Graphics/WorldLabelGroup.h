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

namespace openblack::graphics
{

/// The groups of the labels in the world. The game adds each group to its sort of the things that blend at a different
/// point of the frame, in this order. Thus at an equal distance a group comes after the things that the game added
/// before it.
enum class WorldLabelGroup : uint8_t
{
	/// The bubbles of the chat over the creatures. Each creature adds its bubble to the sort when the land draws it.
	ChatBubbles,
	/// The words over the learning marks. The game draws them at once at the end of the land's objects, after the reward
	/// chests and before the mists, the globes and the icons of the village centres.
	LearningWords,
	/// The numbers that float up from the things that change, added after the columns over the village centres and
	/// before the rain
	FloatingNumbers,
	/// The names over the villagers, added after the rain and the snowfall
	VillagerNames,
	/// The bubble of the advisors' tips, added after the names
	TipBubble,
};

/// How many groups of labels there are
inline constexpr std::size_t k_WorldLabelGroupCount = 5;

/// The groups of labels that a frame drew and that are not submitted yet. Each group submits one time in a frame.
class WorldLabelSubmits
{
public:
	/// The frame drew each group, and no group is submitted yet
	void Drawn() noexcept { _waiting.fill(true); }
	/// True if the group waits to be submitted. The group then waits no more.
	[[nodiscard]] bool Take(WorldLabelGroup group) noexcept
	{
		auto& waiting = _waiting.at(static_cast<std::size_t>(group));
		const bool taken = waiting;
		waiting = false;
		return taken;
	}

private:
	std::array<bool, k_WorldLabelGroupCount> _waiting {};
};

} // namespace openblack::graphics
