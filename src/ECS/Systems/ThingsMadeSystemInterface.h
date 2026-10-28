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

namespace openblack::ecs::systems
{

/// The one count of every thing of the world that the game makes, from the land's load on: buildings, villagers,
/// animals, creatures, trees, features, piles, seeds and the other things that stand or move on the land. Towns,
/// forests, flocks, weather and the like are not things of the world and are not counted. Each thing takes its place
/// in the order as it is made. A new land starts the count again at 0, and a saved game keeps the count.
class ThingsMadeSystemInterface
{
public:
	virtual ~ThingsMadeSystemInterface() = default;

	/// Gives a new thing its place in the order, the number of things made before it, and counts it
	[[nodiscard]] virtual uint32_t TakeNext() = 0;
	/// How many things were made so far: the place of the next thing
	[[nodiscard]] virtual uint32_t Made() const = 0;
};

} // namespace openblack::ecs::systems
