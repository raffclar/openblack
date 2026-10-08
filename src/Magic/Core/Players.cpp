/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Players.h"

#include <cstdio>
#include <cstdlib>

#include "Debug/StateHash.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Villager/VillagerDeath.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Worship/PlayerSpellIcons.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
constexpr size_t k_Players = static_cast<size_t>(PlayerNames::_COUNT);

/// The player state that outlives a land, or that has no entity to live on (Locator::playerSystem)
ecs::systems::PlayerSystemInterface& PlayerState()
{
	if (!Locator::playerSystem::has_value())
	{
		std::fputs("magic::players: no player system in the locator (Locator::playerSystem)\n", stderr);
		std::abort();
	}
	return Locator::playerSystem::value();
}

/// (inferred: guard) the original indexes directly; a bad MAGIC_TYPE falls to 0
size_t TypeIndex(MagicType type)
{
	const auto index = static_cast<size_t>(type);
	return index < PlayerMagic::k_MagicTypes ? index : 0;
}
} // namespace

entt::entity magic::players::EntityOf(PlayerNames player)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity found = entt::null;
	registry.Each<const Player>([&](entt::entity entity, const Player& component) {
		if (found == entt::null && component.name == player)
		{
			found = entity;
		}
	});
	return found;
}

PlayerMagic& magic::players::MagicOf(PlayerNames player)
{
	const auto entity = EntityOf(player);
	if (entity != entt::null)
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (auto* magic = registry.TryGet<PlayerMagic>(entity); magic != nullptr)
		{
			return *magic;
		}
	}
	return PlayerState().MagicWithoutEntity(player);
}

Alignment& magic::players::AlignmentOf(PlayerNames player)
{
	// 0 for a new game (the original takes the profile's, clamped -1..1, 0 without one: no profile in openblack);
	// loading another land does not touch it, so it is kept per PlayerNames, outside the land's registry
	return PlayerState().Alignment(player);
}

bool magic::players::IsHuman(PlayerNames player)
{
	return MagicOf(player).playerType == 1;
}

float magic::players::TribalPower(const GMagicEffectInfo& effect, const PlayerNames* player)
{
	if (player == nullptr)
	{
		return magic::GetTribalPower(effect, nullptr);
	}
	return magic::GetTribalPower(effect, &MagicOf(*player).tribalPower);
}

bool magic::players::IsMagicTypeEnabled(PlayerNames player, MagicType type)
{
	const auto& magic = MagicOf(player);
	return magic.allMagicCheat || magic.remainder[TypeIndex(type)] != 0;
}

void magic::players::SetMagicTypeEnabled(PlayerNames player, MagicType type, bool on)
{
	auto& magic = MagicOf(player);
	auto& remainder = magic.remainder[TypeIndex(type)];
	if (on)
	{
		++remainder;
		magic.everEnabled[TypeIndex(type)] = true;
	}
	else if (remainder > 0)
	{
		--remainder;
	}
	// on or off, it ends by refreshing the icons of the citadel's six worship sites
	worship::player::OnMagicTypesChanged(player);
}

void magic::players::SetMagicTypeEverBeenEnabled(PlayerNames player, MagicType type)
{
	MagicOf(player).everEnabled[TypeIndex(type)] = true;
}

bool magic::players::HasMagicTypeEverBeenEnabled(PlayerNames player, MagicType type)
{
	return MagicOf(player).everEnabled[TypeIndex(type)];
}

uint32_t magic::players::WorldPopulation()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return 0;
	}
	// the Villager entities not counted out (dying takes them off; corpses stay)
	uint32_t count = 0;
	Locator::entitiesRegistry::value().Each<const Villager>([&count](entt::entity villager, const Villager&) {
		if (!ecs::villager::IsCountedOut(villager))
		{
			++count;
		}
	});
	return count;
}

float magic::players::BelieverFraction(PlayerNames player)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return 0.0f;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	// the player's towns: their women and men summed as integers
	uint32_t women = 0;
	uint32_t men = 0;
	for (const auto entity : ecs::map_cells::TownsOf(player))
	{
		if (const auto* town = registry.TryGet<const Town>(entity); town != nullptr)
		{
			women += town->stats.females;
			men += town->stats.males;
		}
	}
	// world != 0 and men + women != 0 -> a float division of the exact counts
	const uint32_t world = WorldPopulation();
	const uint32_t believers = men + women;
	if (world == 0 || believers == 0)
	{
		return 0.0f;
	}
	return static_cast<float>(believers) / static_cast<float>(world);
}

void magic::players::Reset()
{
	PlayerState().ClearMagicWithoutEntity();
	// the last land's player entities go with its registry: the new land lists the ones it makes
	PlayerState().ClearPlayers();
	// the alignment is not reset: it lives with the player, not with the land (AlignmentOf)
}

void magic::players::RegisterStateHash()
{
	state_hash::Register("player_alignment", [](state_hash::Hasher& h) {
		for (size_t p = 0; p < k_Players; ++p)
		{
			const auto& alignment = AlignmentOf(static_cast<PlayerNames>(p));
			h.Float(alignment.value);
			h.Float(alignment.pending);
		}
	});
}
