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
// top of the tallest still thing beneath it. Any other such seed vanishes as it lands, inside its falling effect. The
// forest miracle's shown seed is hidden under the land until the forest is planted, then rides over the tallest of its
// trees as they grow. Out of its player's influence such a seed's miracle closes down. The hand of the seed's player can
// take the shown seed back.

#define LOCATOR_IMPLEMENTATIONS

#include <vector>

#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/HelpProfileSystemInterface.h"
#include "ECS/Systems/MiracleFxSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/SpellLifetime.h"
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

magic::SeedWithSpell MagicSystem::SeedWithSpellOf(const SpellSeed& seed, const Spell& spell) const
{
	const auto& info = magic::GetSpellSeedInfo(Info(), seed.seedType);
	return {.castInHand = info.castType == SpellCastType::SpellCastInHand,
	        .keptInHand = info.isKeptInHand != 0,
	        .followsSpell = seed.followsSpell && info.seedFollowsSpell != 0,
	        .hasIcon = seed.hasIcon,
	        .spellClosedDown = spell.closedDown};
}

bool MagicSystem::SeedShownWithSpell(const SpellSeed& seed, const Spell& spell) const
{
	return magic::ShownWithSpell(SeedWithSpellOf(seed, spell));
}

void MagicSystem::KeepSeedInHand(const Spell& spell)
{
	auto& registry = EntityRegistry();
	// Only a seed in the hand of a player, which these rules act on in that hand
	const auto holder = HolderOf(spell.seed);
	if (!holder.has_value() || !registry.Valid(spell.seed))
	{
		return;
	}
	const ActingAs acting(*this, *holder);
	const auto& info = magic::GetSpellSeedInfo(Info(), registry.Get<const SpellSeed>(*Held()).seedType);
	// Only a phial kept in the hand after its cast (a kept seed that is not cast in the hand) has these rules
	if (info.isKeptInHand == 0 || info.castType == SpellCastType::SpellCastInHand)
	{
		return;
	}
	// It goes when its miracle has closed down; it leaves the hand, still linked, once its miracle is old enough
	if (spell.closedDown)
	{
		DeleteSeed(*Held());
	}
	else if (magic::SeedLeavesHandWithSpell(spell.spellClass, spell.age))
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Magic: the phial leaves the hand {} seconds after its cast, still linked",
		                    spell.age);
		SeedOutOfHandWithSpell(false);
	}
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
	// A seed in the hand is where the reports of its player put the hand; the frames draw it at the hand of this computer
	auto position = transform->position;
	if (const auto holder = HolderOf(spell.seed); holder.has_value())
	{
		position = ReportedHandOf(*holder).value_or(position);
	}
	else
	{
		// Out of the hand, only the frames move a seed that follows its miracle
		position = magic::PlaceForInfluence(position, seed->followsSpell, spell.position);
	}
	if (magic::ClosesOutOfInfluence(SeedShownWithSpell(*seed, spell), _world.InInfluence(seed->player, position),
	                                spell.closedDown))
	{
		CloseDown(spellEntity);
	}
}

void MagicSystem::ShowSeedsWithSpellsTurn()
{
	auto& registry = EntityRegistry();
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	std::vector<entt::entity> seeds;
	registry.Each<const SpellSeed>([&seeds](entt::entity entity, const SpellSeed& seed) {
		if (seed.followsSpell)
		{
			seeds.push_back(entity);
		}
	});
	// The turn decides whether a seed out of the hand shows over its miracle: it has the seed's model or not
	for (const auto entity : seeds)
	{
		const auto& seed = registry.Get<const SpellSeed>(entity);
		const auto* spell = FindSpell(seed.spell);
		if (!registry.AllOf<Transform>(entity) || spell == nullptr)
		{
			continue;
		}
		if (!SeedShownWithSpell(seed, *spell))
		{
			registry.Remove<Mesh>(entity);
			continue;
		}
		if (!registry.AllOf<Mesh>(entity))
		{
			const auto& info = magic::GetSpellSeedInfo(Info(), seed.seedType);
			registry.Assign<Mesh>(entity, resources::HashIdentifier(info.mesh), static_cast<int8_t>(0), static_cast<int8_t>(0));
		}
		// Drawn at its model's own size, unturned
		auto& transform = registry.Get<Transform>(entity);
		transform.rotation = glm::mat3(1.0f);
		transform.scale = glm::vec3(1.0f);
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
		// Only a seed that the turn shows has its model
		if (!registry.AllOf<Mesh>(entity))
		{
			continue;
		}
		const auto& info = magic::GetSpellSeedInfo(Info(), seed.seedType);
		// Its reach across the land is its model's at the seed's size
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

bool MagicSystem::TakeSeedBack(PlayerNames player, entt::entity entity)
{
	auto& registry = EntityRegistry();
	const ActingAs acting(*this, player);
	if (!HasActingHand() || Held().has_value() || !registry.Valid(entity) || !registry.AllOf<SpellSeed, Transform>(entity) ||
	    !Locator::infoConstants::has_value())
	{
		return false;
	}
	auto& seed = registry.Get<SpellSeed>(entity);
	const bool own = ActsOnOwnHand();
	// The seed keeps its power-up level. It holds nothing past the cost of that level after its cast, so it gives nothing
	// back to its worship site. The local hand shows the name of the miracle at once.
	if (own)
	{
		AnnounceHeldMiracle(seed);
	}
	auto* spell = FindSpell(seed.spell);
	bool enough = true;
	if (spell != nullptr)
	{
		enough = StoreFromSpell(entity, seed, *spell);
	}
	else
	{
		ForgetStoredSpell(seed);
	}
	const auto takeBack = magic::TakeBackOf(spell != nullptr, enough);
	if (own && Locator::helpProfileSystem::has_value())
	{
		Locator::helpProfileSystem::value().Trigger(takeBack.helpEvent);
	}
	if (!takeBack.kept)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Magic: the hand of player {} takes back a seed with too little left; it goes",
		                    static_cast<int>(player));
		DeleteSeed(entity);
		return false;
	}

	// The seed no longer stays with a miracle. It is ready at once, and the hand holds it at its own size from the hand.
	const auto& info = magic::GetSpellSeedInfo(Info(), seed.seedType);
	seed.followsSpell = false;
	seed.ready = true;
	seed.turnsInHand = 0;
	// The seed counts as a seed that did not cast, so a dispenser of its player takes it again. The mark of the cast's
	// link goes.
	seed.castBefore = false;
	seed.linkedByCast = false;
	auto& transform = registry.Get<Transform>(entity);
	transform.rotation = glm::mat3(1.0f);
	transform.scale = glm::vec3(info.scale);
	transform.position = own ? _hand.handPosition : ReportedHandOf(player).value_or(transform.position);
	SetHeld(entity);
	// The player's last miracle is now this one: the R gesture and the R key ask for it again
	registry.Each<Player>([player, &seed](entt::entity, Player& record) {
		if (record.name == player)
		{
			record.lastSeedGained = seed.seedType;
		}
	});
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Magic: the hand of player {} takes back its seed, with {} prayer power left",
	                    static_cast<int>(player), seed.storedChants);
	// The hand of another player shows no effect on this computer
	if (!own)
	{
		ShowHeldSeed();
		return true;
	}
	_input = {};
	_pour = {};
	StartHandEffect(entity);
	ShowHeldSeed();
	if (Locator::miracleFxSystem::has_value())
	{
		auto& fx = Locator::miracleFxSystem::value();
		fx.SeedInHand(seed.powerUp, seed.powerUp);
		const auto type = magic::GetMagicTypeFromPowerUpLevel(info, seed.powerUp);
		if (const auto tribe = TribalPowerTribe(player, type); tribe.has_value())
		{
			fx.StartTribalPowerRing(*tribe);
		}
	}
	return true;
}
