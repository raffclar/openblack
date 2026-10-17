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

#include <string>
#include <string_view>
#include <vector>

namespace openblack::ecs::systems
{

/// The saved games of the current player. A slot keeps the land, the turn and the creatures: each creature's part of
/// the world, its mind file and its player's leash on it. A save is not complete yet: the towns, the villagers, the
/// story and the scripts are not in it. A load puts the creatures of the land that is open back as the save keeps them.
class SaveGameSystemInterface
{
public:
	struct Slot
	{
		uint32_t slot {0};
		uint32_t land {0};
		std::u16string date;
		std::u16string name;
	};

	virtual ~SaveGameSystemInterface() = default;

	/// Saves the game into the slot with the name. Returns whether it wrote every file.
	virtual bool Save(uint32_t slot, std::u16string_view name) = 0;
	/// Loads the creatures of the slot into the land that is open. Returns whether the slot's land is open and the
	/// slot has a creature that the land has.
	virtual bool Load(uint32_t slot) = 0;
	/// The slots that have a save, for the current land's land folders
	[[nodiscard]] virtual std::vector<Slot> Slots() const = 0;
};

} // namespace openblack::ecs::systems
