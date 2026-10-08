/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EffectValues.h"

#include <cmath>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Alignment.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Influence.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownEmergency.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerScript.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Players.h"
#include "Reactions.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::effects;
using namespace openblack::ecs::components;

namespace
{
float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// The info's defenceMultiplier* (a creature's are its row's; its own override is not ported). Without an info: 1
/// (inferred)
std::array<float, static_cast<size_t>(EffectValues::Number::_COUNT)> DefenceMultipliers(entt::entity object)
{
	const auto* info = fire::traits::InfoOf(object); // the physics' infos plus abodes, fields, features, fireballs
	if (info == nullptr)
	{
		std::array<float, static_cast<size_t>(EffectValues::Number::_COUNT)> ones {};
		ones.fill(1.0f);
		return ones;
	}
	return {info->defenceMultiplierBurn,
	        info->defenceMultiplierCrush,
	        info->defenceMultiplierHit,
	        info->defenceMultiplierHeal,
	        info->defenceMultiplierFlyAway,
	        info->defenceMultiplierAlignmentModification,
	        info->defenceMultiplierBeliefModification};
}

/// The burn goes to the fire first (fire::ApplyEffectToFireEffectIfNecessary, ECS/Fire), then the positive crush and
/// hit x their multipliers
float DamageEffect(entt::entity object, const EffectValues& values)
{
	fire::ApplyEffectToFireEffectIfNecessary(object, values);
	if (!ecs::IsAvailable(object))
	{
		return 0.0f;
	}
	const auto multipliers = DefenceMultipliers(object);
	float damage = 0.0f;
	for (const size_t i : {static_cast<size_t>(EffectValues::Number::Crush), static_cast<size_t>(EffectValues::Number::Hit)})
	{
		const float amount = values.numbers[i] * multipliers[i];
		if (amount > 0.0f)
		{
			damage += amount;
		}
	}
	return damage;
}

/// The positive heal x its multiplier
float HealEffect(entt::entity object, const EffectValues& values)
{
	constexpr auto k_Heal = static_cast<size_t>(EffectValues::Number::Heal);
	const float amount = values.numbers[k_Heal] * DefenceMultipliers(object)[k_Heal];
	return amount > 0.0f ? amount : 0.0f;
}

/// The town of an effect receiver: an abode's (abode_villagers::TownOf) and a villager's. (approximate) none for the
/// other classes
entt::entity TownOfObject(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Abode>(object))
	{
		return abode_villagers::TownOf(object);
	}
	if (const auto* villager = registry.TryGet<const Villager>(object); villager != nullptr)
	{
		return registry.Valid(villager->town) ? villager->town : entt::null;
	}
	return entt::null;
}

/// The player of an effect receiver: a villager's town's owner (villager::GetPlayerOf); the others NEUTRAL (inferred:
/// not ported per class yet)
PlayerNames PlayerOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Villager>(object))
	{
		return villager::GetPlayerOf(object).value_or(PlayerNames::NEUTRAL);
	}
	return PlayerNames::NEUTRAL;
}

/// A kill by an effect, with the effect's player and damage: a villager -> VillagerDead(2 SPELL)
/// (ECS/Villager/VillagerDeath.h); an animal -> dying (ECS/AnimalAI). TODO: abodes and the other classes.
void DestroyedByEffect(entt::entity object, const EffectValues& values, float damage)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Villager>(object))
	{
		villager::DestroyedByEffect(object, values.player, damage);
	}
	else if (registry.AllOf<Animal>(object))
	{
		ecs::animal_ai::DestroyedByEffect(object);
	}
}
} // namespace

EffectValues EffectValues::FromEffectInfo(const GEffectInfo& info)
{
	EffectValues values;
	values.numbers = {info.effectBurn,
	                  info.effectCrush,
	                  info.effectHit,
	                  info.effectHeal,
	                  info.effectFlyAway,
	                  info.effectAlignmentModification,
	                  info.effectBeliefModification};
	values.radius = info.radius;
	return values;
}

void EffectValues::Scale(float factor)
{
	if (factor == 1.0f)
	{
		return;
	}
	for (auto& number : numbers)
	{
		number *= factor;
	}
}

bool EffectValues::IsDestructive() const
{
	return numbers[static_cast<size_t>(Number::Burn)] > 0.0f || numbers[static_cast<size_t>(Number::Crush)] > 0.0f ||
	       numbers[static_cast<size_t>(Number::Hit)] > 0.0f || numbers[static_cast<size_t>(Number::FlyAway)] > 0.0f;
}

bool effects::IsEffectReceiver(entt::entity object, const EffectValues& /*values*/)
{
	// A villager takes a heal only while alive (dead villagers are gone from openblack's world); otherwise whether it is
	// reachable (unverified: taken as yes). Any other object: yes.
	return ecs::IsAvailable(object);
}

float effects::ConvertTemperatureToDamage(entt::entity object, float temperature)
{
	const auto* info = fire::traits::InfoOf(object);
	if (info == nullptr)
	{
		return 0.0f;
	}
	// the combustion temperature
	const float combustion = info->combustionTemperature;
	if (temperature < combustion)
	{
		return 0.0f;
	}
	return (temperature - combustion) / combustion * info->defenceMultiplierBurn * 0.1f;
}

std::array<float, static_cast<size_t>(EffectValues::Number::_COUNT)> effects::GetDefenseMultiplier(entt::entity object)
{
	return DefenceMultipliers(object);
}

float effects::ApplyEffect(entt::entity object, EffectValues& values)
{
	const float life0 = life::LifeOf(object);
	// every abode class (houses, storage pit, town centre, creche, workshop ...)
	const bool isAbode = Locator::entitiesRegistry::value().AllOf<Abode>(object);
	const float damage = DamageEffect(object, values);
	const float heal = HealEffect(object, values);
	float result = 0.0f;
	if (heal > 0.0f)
	{
		result = (1.0f - life0) / heal;
		// an abode's own increase (ecs::abodes: RestartBeingFunctional when it crosses the threshold); the generic one for
		// the rest
		if (isAbode)
		{
			abodes::IncreaseLife(object, heal);
		}
		else
		{
			life::IncreaseLife(object, heal);
		}
	}
	if (damage > 0.0f)
	{
		result += life0 / damage;
		// With the effect's player: an abode's own reduction (ecs::abodes: the multi-map fixed part, the building site,
		// StopBeingFunctional, the town's emergency); a field's (no change, no site), as fields carry an Abode too:
		// abodes::ReduceLife handles both; a creature's own, not ported yet, so it takes no harm; the generic one for the
		// rest
		if (isAbode)
		{
			abodes::ReduceLife(object, damage, values.player);
		}
		else if (Locator::entitiesRegistry::value().AllOf<Creature>(object))
		{
			if (auto logger = spdlog::get("game"); logger != nullptr)
			{
				SPDLOG_LOGGER_DEBUG(logger, "Effect: creature {} takes no harm from a spell (not ported)",
				                    static_cast<uint32_t>(object));
			}
		}
		else
		{
			life::ReduceLife(object, damage);
		}
	}
	auto& registry = Locator::entitiesRegistry::value();
	const bool killed = ecs::IsAvailable(object) && life::LifeOf(object) == 0.0f && life0 != 0.0f;
	// Crushed: REACT_TO_OBJECT_CRUSHED from the applier (or the object), if the object started none (only what can be
	// crushed; inferred: villagers)
	if (values.numbers[static_cast<size_t>(EffectValues::Number::Crush)] > 0.01f && registry.AllOf<Villager>(object) &&
	    reactions::GetReactionInitiatedBy(object) == 0)
	{
		const auto initiator = values.appliedBy != entt::null ? values.appliedBy : object;
		// a crushed reaction for the object's player
		reactions::CreateReaction(initiator, Reaction::ReactToObjectCrushed, PlayerOf(object), true);
	}
	// A destructive effect on an object of a town, with an applier and a non-zero aggression (the burn converted to
	// damage + the damage) updates the town's aggressor with the caused player. Only its record is ported
	// (town_emergency::UpdateAggressor)
	if (values.IsDestructive() && registry.Valid(object) && values.appliedBy != entt::null && registry.Valid(values.appliedBy))
	{
		const float burn = values.numbers[static_cast<size_t>(EffectValues::Number::Burn)];
		const float aggression = ConvertTemperatureToDamage(object, burn) + damage;
		if (const auto town = TownOfObject(object); town != entt::null && aggression != 0.0f)
		{
			const auto caused = values.causedPlayer.has_value() ? values.causedPlayer : values.player;
			town_emergency::UpdateAggressor(town, caused);
		}
	}
	// Whose alignment moves: the creature's or the caster player's. The per-player damage statistic is not kept.
	// TODO: the applying creature's kill counter on a kill, and the creature's own alignment when a creature applies
	// the effect: today a creature-applied effect moves no alignment (openblack deletes a dead villager at once, so the
	// alignment is read before DestroyedByEffect)
	if (!values.appliedByCreature && values.player.has_value())
	{
		alignment::Update(magic::players::AlignmentOf(*values.player), object, values, life0);
	}
	if (killed)
	{
		DestroyedByEffect(object, values, damage);
	}
	return result;
}

entt::entity EffectValues::ApplyEffectToMapPos(const glm::vec3& position)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the corners: the metres -/+ radius, back to map coords; the cells are their signed high words and each one is
	// tested in bounds, so a corner off the map does not wrap
	const auto corner = [](float metres, float offset) {
		return static_cast<int32_t>(map_coords::SignedCellOf(map_coords::ToFixedGUtils(map_coords::Quantise(metres) + offset)));
	};
	const glm::ivec2 low(corner(position.x, -radius), corner(position.z, -radius));
	const glm::ivec2 high(corner(position.x, radius), corner(position.z, radius));
	const float altitude = LandAt(position.x, position.z) + position.y;
	entt::entity hit = entt::null;
	for (int32_t x = low.x; x <= high.x; ++x)
	{
		for (int32_t z = low.y; z <= high.y; ++z)
		{
			if (!map_coords::InBounds(glm::ivec2(x, z)))
			{
				continue;
			}
			// the cell's fixed list, then its mobile one (ecs::map_cells). No "done" set and no own-cell test: a multi-cell
			// object is offered the effect once per cell of the square it is in (inferred: unless ApplyEffect limits it
			// itself)
			for (const auto object : map_cells::ObjectsInCell(glm::ivec2(x, z)))
			{
				// available (not being deleted, ecs::IsAvailable; a villager whose final state is not DYING), then an effect
				// receiver
				if (!ecs::IsAvailable(object) || (registry.AllOf<Villager>(object) && !villager::IsAvailable(object)) ||
				    !IsEffectReceiver(object, *this))
				{
					continue;
				}
				const auto* transform = registry.TryGet<const Transform>(object);
				if (transform == nullptr)
				{
					continue;
				}
				// the fire centre (the position; a dead tree's mesh centre) and fire radius (the 2D radius; a dead tree's 0.35
				// x its height), ECS/Fire/FireObjectTraits
				const glm::vec3 centre = fire::traits::FireCentre(object);
				// the distance of the position and that centre against the radius sum
				const float distance = gutils::GetDistanceInMetres(position, centre);
				if (fire::traits::DefaultFireRadius(object) + radius < distance)
				{
					continue;
				}
				// the height + the radius against the altitude difference
				if (object::GetHeight(object) + radius < std::abs(altitude - (LandAt(centre.x, centre.z) + centre.y)))
				{
					continue;
				}
				// the object itself takes the effect
				ApplyEffect(object, *this);
				hit = object;
			}
		}
	}
	return hit;
}
