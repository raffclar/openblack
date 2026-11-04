/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the objects of a miracle in the world do for the miracle, and the data for the debug window.

#define LOCATOR_IMPLEMENTATIONS

#include <optional>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Transform.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"
#include "Magic/DispenserRules.h"
#include "MagicSystem.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}
} // namespace

// What the miracles' objects in the world do for them

void MagicSystem::ShieldStruck(const Spell& shield, bool destroyed)
{
	if (Locator::magicShieldSystem::has_value())
	{
		Locator::magicShieldSystem::value().ShieldStruck(EntityRegistry().ToEntity(shield), destroyed);
	}
}

bool MagicSystem::HasWorldObjects(const Spell& spell) const
{
	const auto entity = EntityRegistry().ToEntity(spell);
	if (spell.spellClass == magic::SpellClass::Shield)
	{
		return Locator::magicShieldSystem::has_value() && Locator::magicShieldSystem::value().HasObject(entity);
	}
	if (spell.spellClass == magic::SpellClass::Forest)
	{
		return Locator::forestSystem::has_value() && Locator::forestSystem::value().HasTrees(entity);
	}
	return false;
}

bool MagicSystem::PlantForest(Spell& spell)
{
	return Locator::forestSystem::has_value() &&
	       Locator::forestSystem::value().Plant(EntityRegistry().ToEntity(spell), spell, TribalPower(spell)) > 0;
}

bool MagicSystem::ForestCanGrowAt(glm::vec3 point) const
{
	return !Locator::forestSystem::has_value() || Locator::forestSystem::value().CanGrowAt(point);
}

bool MagicSystem::SendSpellEvent(entt::entity spell, const particles::SpellEventInfo& event)
{
	return OnSpellEvent(spell, event);
}

float MagicSystem::ForcePayForSpell(entt::entity entity, float cost)
{
	auto* spell = FindSpell(entity);
	if (spell == nullptr)
	{
		return 0.0f;
	}
	return magic::PayFor(spell->chants, magic::spells::RulesOf(*this, *spell), CasterOf(*spell), cost, magic::Refill::Whole);
}

void MagicSystem::SetSpellRadius(entt::entity entity, float radius)
{
	auto* spell = Locator::entitiesRegistry::value().TryGet<Spell>(entity);
	if (spell == nullptr)
	{
		return;
	}
	spell->magnitude = radius;
	if (Locator::effectRequestSystem::has_value() && Locator::effectRequestSystem::value().IsRunning(spell->effect))
	{
		Locator::effectRequestSystem::value().SetMagnitude(spell->effect, radius);
	}
	if (Locator::magicShieldSystem::has_value())
	{
		Locator::magicShieldSystem::value().FollowSpellRadius(entity, radius);
	}
}

float MagicSystem::SpellStrength(entt::entity entity)
{
	const auto* spell = FindSpell(entity);
	return spell != nullptr ? magic::spells::StrengthOf(*this, *spell) : 0.0f;
}

// For the debug window

void MagicSystem::StartCastEffect(Spell& spell, ParticleType type, bool synced)
{
	// It plays out by itself, as big as the miracle and heading the way it was cast
	spell.castEffect = Locator::effectRequestSystem::value().Start({
	    .type = type,
	    .position = spell.castPosition,
	    .magnitude = spell.magnitude,
	    .synced = synced,
	    .player = static_cast<int>(spell.caster.player),
	    .direction = spell.direction,
	    .processInfo = spell.processInfo,
	});
}

void MagicSystem::RainOnFire(const glm::vec3& point)
{
	if (!Locator::reactionSystem::has_value())
	{
		return;
	}
	auto& reactions = Locator::reactionSystem::value();
	auto& registry = EntityRegistry();
	bool done = false;
	registry.Each<Spell>([&](entt::entity entity, Spell& spell) {
		if (done || spell.spellClass != magic::SpellClass::StormAndTornado)
		{
			return;
		}
		auto& watching = _rainWatchers[entity];
		if (watching != 0 && reactions.IsActive(watching))
		{
			return;
		}
		if (glm::distance(glm::vec2(point.x, point.z), glm::vec2(spell.position.x, spell.position.z)) < spell.magnitude)
		{
			watching = reactions.Create({.initiator = entity,
			                             .type = Reaction::ReactToMagicWaterPuttingOutFire,
			                             .player = spell.caster.player,
			                             .position = spell.position,
			                             .onCast = true});
			done = true;
		}
	});
}

std::optional<entt::entity> MagicSystem::SpellAt(MagicType type, glm::vec3 point, float radius) const
{
	// The miracles are kept newest first
	for (const auto entity : _spells)
	{
		const auto* spell = EntityRegistry().TryGet<const Spell>(entity);
		if (spell != nullptr && spell->magicType == type && gutils::GetDistanceInMetres(spell->position, point) < radius)
		{
			return entity;
		}
	}
	return std::nullopt;
}

std::vector<MagicSystemInterface::SpellInfo> MagicSystem::GetSpells() const
{
	std::vector<SpellInfo> result;
	if (!Locator::infoConstants::has_value())
	{
		return result;
	}
	auto& self = const_cast<MagicSystem&>(*this);
	for (const auto entity : _spells)
	{
		const auto* spell = EntityRegistry().TryGet<const Spell>(entity);
		if (spell == nullptr)
		{
			continue;
		}
		result.push_back({
		    .entity = entity,
		    .magicType = spell->magicType,
		    .player = spell->caster.player,
		    .age = spell->age,
		    .duration = spell->duration,
		    .chants = spell->chants.chants,
		    .initialChants = spell->chants.initialChants,
		    .strength = magic::spells::StrengthOf(self, *spell),
		    .upkeep = magic::spells::CostToMaintain(Info(), *spell),
		    .closing = spell->closedDown,
		    .fromHand = spell->castFromHand,
		    .effect = spell->effect,
		    .position = spell->position,
		});
	}
	return result;
}

std::vector<MagicSystemInterface::DispenserInfo> MagicSystem::GetDispensers() const
{
	std::vector<DispenserInfo> result;
	EntityRegistry().Each<const SpellDispenser, const Transform>(
	    [&](entt::entity entity, const SpellDispenser& dispenser, const Transform& transform) {
		    result.push_back({
		        .entity = entity,
		        .magicType = dispenser.magicType,
		        .position = transform.position,
		        .hasOrb = dispenser.orb != entt::null,
		        .tick = dispenser.timer.tick,
		        .period = dispenser.timer.period,
		        .active = dispenser.timer.active,
		    });
	    });
	return result;
}
