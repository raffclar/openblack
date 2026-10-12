/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A seed that leaves the hand bound to the miracle it cast (the storms', the shields' and the forest's) is shown over
// its miracle when it was made at an icon of the player's worship: its model, at its own size and unturned, sitting on
// top of the tallest still thing beneath it. The forest miracle's seed is hidden under the land until the forest is
// planted, then rides over the tallest of its trees as they grow. Out of its player's influence such a seed's miracle
// closes down.

#define LOCATOR_IMPLEMENTATIONS

#include <vector>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/SpellSeedRules.h"
#include "MagicSystem.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}

/// How high over the land its top stands, the land under it measured where it stands
float TopOverLand(const glm::vec3& position, float height, const LandIslandInterface& land)
{
	return position.y - land.GetHeightAt({position.x, position.z}) + height;
}

/// What stands in a point's cell of the map, as the shown seed may sit on it
std::vector<magic::ThingUnderSeed> ThingsUnder(entt::entity seed, const glm::vec3& point, const LandIslandInterface& land)
{
	std::vector<magic::ThingUnderSeed> things;
	if (!Locator::entitiesMap::has_value())
	{
		return things;
	}
	auto& registry = EntityRegistry();
	const auto& map = Locator::entitiesMap::value();
	const auto add = [&](std::span<const entt::entity> cell) {
		for (const auto thing : cell)
		{
			if (thing == seed || !registry.Valid(thing))
			{
				continue;
			}
			const auto* transform = registry.TryGet<const Transform>(thing);
			if (transform == nullptr)
			{
				continue;
			}
			const auto size = ecs::world_objects::SizeOf(thing);
			things.push_back({
			    .offset = {transform->position.x - point.x, transform->position.z - point.z},
			    .radius = size.radius,
			    .top = TopOverLand(transform->position, size.height, land),
			    .living = registry.AnyOf<Villager, Animal, Creature>(thing),
			    .moving = registry.AllOf<InPhysics>(thing),
			});
		}
	};
	add(map.GetFixedInGridCell(point));
	add(map.GetMobileInGridCell(point));
	return things;
}
} // namespace

bool MagicSystem::SeedShownWithSpell(const SpellSeed& seed, const Spell& spell) const
{
	const auto& info = magic::GetSpellSeedInfo(Info(), seed.seedType);
	return magic::ShownWithSpell({.castInHand = info.castType == SpellCastType::SpellCastInHand,
	                              .keptInHand = info.isKeptInHand != 0,
	                              .followsSpell = seed.followsSpell && info.seedFollowsSpell != 0,
	                              .hasIcon = seed.hasIcon,
	                              .spellClosedDown = spell.closedDown});
}

void MagicSystem::KeepSeedInInfluence(entt::entity spellEntity, const Spell& spell)
{
	auto& registry = EntityRegistry();
	if (spell.seed == entt::null || !registry.Valid(spell.seed) || _ignoreInfluence)
	{
		return;
	}
	const auto* seed = registry.TryGet<const SpellSeed>(spell.seed);
	const auto* transform = registry.TryGet<const Transform>(spell.seed);
	if (seed == nullptr || transform == nullptr || seed->spell != spellEntity)
	{
		return;
	}
	if (magic::ClosesOutOfInfluence(SeedShownWithSpell(*seed, spell), _world.InInfluence(seed->player, transform->position),
	                                spell.closedDown))
	{
		CloseDown(spellEntity);
	}
}

void MagicSystem::ShowSeedsWithSpells()
{
	auto& registry = EntityRegistry();
	if (!Locator::infoConstants::has_value() || !Locator::terrainSystem::has_value())
	{
		return;
	}
	const auto& land = Locator::terrainSystem::value();
	std::vector<entt::entity> seeds;
	registry.Each<const SpellSeed>([&seeds](entt::entity entity, const SpellSeed& seed) {
		if (seed.followsSpell)
		{
			seeds.push_back(entity);
		}
	});
	for (const auto entity : seeds)
	{
		const auto& seed = registry.Get<const SpellSeed>(entity);
		auto* transform = registry.TryGet<Transform>(entity);
		const auto* spell = FindSpell(seed.spell);
		if (transform == nullptr || spell == nullptr)
		{
			continue;
		}
		// It stays with its miracle across the land
		transform->position.x = spell->position.x;
		transform->position.z = spell->position.z;
		if (!SeedShownWithSpell(seed, *spell))
		{
			registry.Remove<Mesh>(entity);
			continue;
		}
		const auto& info = magic::GetSpellSeedInfo(Info(), seed.seedType);
		if (!registry.AllOf<Mesh>(entity))
		{
			registry.Assign<Mesh>(entity, resources::HashIdentifier(info.mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
		}
		// Drawn at its model's own size, unturned; its reach across the land is its model's at the seed's size
		transform->rotation = glm::mat3(1.0f);
		transform->scale = glm::vec3(1.0f);
		const float radius = ecs::world_objects::SizeOf(entity).radius * info.scale;
		const auto things = ThingsUnder(entity, transform->position, land);
		float height = magic::TopUnderSeed(radius, things);
		if (spell->spellClass == magic::SpellClass::Forest && Locator::forestSystem::has_value())
		{
			height = magic::ForestSeedHeight(height, Locator::forestSystem::value().TallestTreeOf(seed.spell));
		}
		transform->position.y = land.GetHeightAt({transform->position.x, transform->position.z}) + height;
	}
}
