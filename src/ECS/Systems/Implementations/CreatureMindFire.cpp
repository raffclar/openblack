/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a creature's mind does about a fire that reaches it: it goes to put the fire out with the water miracle when it
// cares enough for what burns and has seen the miracle often enough, and runs from the burning thing otherwise

#define LOCATOR_IMPLEMENTATIONS

#include <optional>
#include <string_view>

#include <glm/gtx/vec_swizzle.hpp>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureFireReaction.h"
#include "CreatureMindSystem.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/ForestMember.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicWorldInterface.h"
#include "Magic/SpellRules.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using creature_desires::Desire;

namespace
{
/// The game's actions for putting a fire out and running from it
constexpr std::string_view k_PutOutFireAction = "PutOutFireWithMagicWater";
constexpr std::string_view k_RunAwayAction = "RunAwayFromObject";
/// A creature douses what it put out with this many buckets of water's worth
constexpr float k_DouseBuckets = 5.0f;

std::optional<entt::entity> TownOf(const ecs::Registry& registry, uint32_t id)
{
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(id);
	return found != towns.end() && registry.Valid(found->second) ? std::optional(found->second) : std::nullopt;
}

/// The nearest of the land's forests with trees, strictly within a distance of a point, the newest of the nearest
std::optional<entt::entity> NearestForestWithTrees(const ecs::Registry& registry, glm::vec3 point, float within)
{
	if (!Locator::forestSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& forests = Locator::forestSystem::value();
	const auto from = map_coords::FromMetres(glm::xz(point));
	std::optional<entt::entity> nearest;
	for (const auto forest : forests.LandForests())
	{
		const auto* at = registry.TryGet<const Transform>(forest);
		if (at == nullptr)
		{
			continue;
		}
		const auto distance = gutils::GetDistanceInMetres(from, map_coords::FromMetres(glm::xz(at->position)));
		if (distance < within && (!forests.TreesOf(forest, false).empty() || !forests.TreesOf(forest, true).empty()))
		{
			nearest = forest;
			within = distance;
		}
	}
	return nearest;
}
} // namespace

std::optional<entt::entity> CreatureMindSystem::BelongsTo(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(thing))
	{
		return std::nullopt;
	}
	// A creature is its own
	if (registry.AllOf<Creature>(thing))
	{
		return thing;
	}
	// TODO(physics): the game also has a temple's parts belong to the temple and a totem statue to its town; neither is
	// a thing of its own here yet
	if (const auto* villager = registry.TryGet<const Villager>(thing))
	{
		return registry.Valid(villager->town) ? std::optional(villager->town) : std::nullopt;
	}
	if (const auto* abode = registry.TryGet<const Abode>(thing))
	{
		return TownOf(registry, abode->townId);
	}
	if (const auto* field = registry.TryGet<const Field>(thing))
	{
		return field->town >= 0 ? TownOf(registry, static_cast<uint32_t>(field->town)) : std::nullopt;
	}
	if (registry.AllOf<Tree>(thing))
	{
		if (const auto* member = registry.TryGet<const ForestMember>(thing);
		    member != nullptr && Locator::forestSystem::has_value())
		{
			if (const auto forest = Locator::forestSystem::value().LandForestOf(member->forest))
			{
				return forest;
			}
		}
		const auto* at = registry.TryGet<const Transform>(thing);
		return at != nullptr ? NearestForestWithTrees(registry, at->position, creature_fire::k_NearestForestDistance)
		                     : std::nullopt;
	}
	if (const auto* animal = registry.TryGet<const Animal>(thing))
	{
		return registry.Valid(animal->flock) ? std::optional(animal->flock) : std::nullopt;
	}
	return std::nullopt;
}

void CreatureMindSystem::ReactToFire(entt::entity creature, entt::entity burning)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mind = registry.Valid(creature) ? registry.TryGet<const CreatureMindState>(creature) : nullptr;
	if (mind == nullptr || !mind->desires.has_value() || !registry.Valid(burning))
	{
		return;
	}
	// What it weighs is what the burning thing belongs to, by how useful it has learnt that is for compassion
	const auto owner = BelongsTo(burning);
	const auto usefulness =
	    owner.has_value() ? std::optional(ActivityUsefulness(creature, *mind, Desire::Compassion, *owner)) : std::nullopt;
	const auto sightings = MiracleSightings(creature, MagicType::Water);
	const auto response = sightings.has_value() ? creature_fire::Choose(usefulness, (*mind->desires)[Desire::Compassion].value,
	                                                                    sightings->first, sightings->second)
	                                            : creature_fire::Response::RunAway;
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} reacts to the fire on {}: {} (usefulness {}, compassion {:.3f})",
	                    entt::to_integral(creature), entt::to_integral(burning),
	                    response == creature_fire::Response::PutOut ? "puts it out" : "runs away", usefulness.value_or(-1.0f),
	                    (*mind->desires)[Desire::Compassion].value);
	if (response == creature_fire::Response::PutOut)
	{
		ForceActivity(
		    creature,
		    {.desire = Desire::Compassion, .activityObject = owner, .action = k_PutOutFireAction, .actionObject = burning});
		return;
	}
	ForceActivity(creature, {.desire = Desire::Fear, .action = k_RunAwayAction, .actionObject = burning});
}

bool CreatureMindSystem::Douse(entt::entity creature, entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object) || !Locator::magicSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& effects = Locator::infoConstants::value().effect;
	const auto row = static_cast<size_t>(EffectInfo::WaterPail);
	if (row >= effects.size())
	{
		return false;
	}
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "Creature {} douses {}", entt::to_integral(creature), entt::to_integral(object));
	// A bucket of water's effect, five times over, applied by the creature and no player
	auto values = magic::EffectValues::From(effects.at(row));
	values.Scale(k_DouseBuckets);
	Locator::magicSystem::value().ApplyEffectToObject(object, values,
	                                                  magic::EffectSource {.appliedBy = creature, .playerless = true});
	// Its fire out, the creature has seen to what it wanted
	return !registry.Valid(object) || !Locator::fireSystem::has_value() || !Locator::fireSystem::value().IsOnFire(object);
}
