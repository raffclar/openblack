/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "Creature/CaveCreature.h"
#include "ECS/TattooSites.h"

namespace openblack::ecs::components
{

/// The creature that the main menu's tattoo editor shows: a copy (see CreatureCopy) of a body of its own, which is not
/// a creature of the world. It stands at the world's origin and is drawn only in the editor's own view.
struct TattooMenuCreature
{
	/// Its fidgets and where it looks, as the Creature Cave's creature has them while the editor is open on it
	cave_creature::State state;
	/// Where each tattoo place is on the skin of its body, found when the editor first shows it
	std::optional<tattoo_sites::Found> tattooSites;
};

} // namespace openblack::ecs::components
