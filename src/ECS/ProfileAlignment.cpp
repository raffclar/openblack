/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ProfileAlignment.h"

#include "ECS/Components/Alignment.h"
#include "ECS/Components/Player.h"
#include "ECS/Registry.h"

void openblack::profile_alignment::StartGods(ecs::Registry& registry, const SentAlignment& sent)
{
	using namespace ecs::components;
	registry.Each<const Player, Alignment>([&sent](const Player& player, Alignment& alignment) {
		if (player.name == PlayerNames::NEUTRAL)
		{
			return;
		}
		// Only the alignment changes: a change still to come stays
		alignment.value = StartsAt(sent(player.name));
	});
}
