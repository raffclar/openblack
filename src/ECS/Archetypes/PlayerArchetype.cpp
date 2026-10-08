/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerArchetype.h"

#include "ECS/Components/Alignment.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/PlayerMagic.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;

entt::entity PlayerArchetype::Create(PlayerNames name)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Player>(entity, name);
	// the player's magic fields; PLAYER_ONE is the human at this interface (inferred). Its alignment is not on the
	// entity: it lives with the player across lands (the player system, magic::players::AlignmentOf)
	auto& magic = registry.Assign<PlayerMagic>(entity);
	magic.playerType = name == PlayerNames::PLAYER_ONE ? 1 : 0;
	return entity;
}
