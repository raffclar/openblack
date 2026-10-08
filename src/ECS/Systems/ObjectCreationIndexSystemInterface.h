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

namespace openblack::ecs::systems
{
/// The game's object creation counter and the town centres' spell icons it counts (ecs::object_index goes through
/// it)
class ObjectCreationIndexSystemInterface
{
public:
	virtual ~ObjectCreationIndexSystemInterface() = default;

	/// A land is loaded: the counter back to 0, or to 2 on the game's first land; the towns' spells start again
	virtual void OnLoadMap() = 0;
	/// Takes the next index of the original's counter
	[[nodiscard]] virtual uint32_t Next() = 0;
	/// Takes numbers for objects openblack doesn't create, from the same range Next would use
	virtual void Skip(uint32_t count) = 0;
	/// A town's spell seed (a new one of a town with a centre takes an icon); its centre (an icon per seed so far)
	virtual void AddTownSpell(uint32_t town, const std::string& spell) = 0;
	virtual void OnTownCentre(uint32_t town) = 0;
};
} // namespace openblack::ecs::systems
