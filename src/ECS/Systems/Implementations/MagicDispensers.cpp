/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The spell dispensers and the one-shot seeds: how a dispenser is made, charged and set, and how it makes its
// globe.

#define LOCATOR_IMPLEMENTATIONS

#include <chrono>
#include <limits>
#include <optional>

#include <glm/gtx/euler_angles.hpp>

#include "3D/L3DMesh.h"
#include "ECS/Archetypes/SpellSeedArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/FootpathSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/DispenserRules.h"
#include "Magic/MagicTables.h"
#include "MagicSystem.h"
#include "Resources/ResourcesInterface.h"
#include "Sound/SoundRequestInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The kind of building a dispenser is
constexpr auto k_DispenserBuilding = AbodeInfo::NorseSpellDispenser;

ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}

/// The town a new dispenser belongs to: the town asked for, or else the first town that the first player gained. None
/// when that player has no town.
entt::entity DispenserTown(entt::entity requested)
{
	const auto& registry = EntityRegistry();
	if (requested != entt::null && registry.Valid(requested) && registry.AllOf<Town>(requested))
	{
		return requested;
	}
	entt::entity first = entt::null;
	uint32_t earliest = std::numeric_limits<uint32_t>::max();
	registry.Each<const Town>([&](entt::entity entity, const Town& town) {
		if (town.owner == PlayerNames::PLAYER_ONE && town.gained < earliest)
		{
			earliest = town.gained;
			first = entity;
		}
	});
	return first;
}

/// A dispenser joins its town's buildings, as the newest
void JoinTown(entt::entity dispenser, entt::entity town)
{
	auto& registry = EntityRegistry();
	registry.Get<SpellDispenser>(dispenser).town = town;
	if (town != entt::null)
	{
		auto& abodes = registry.Get<Town>(town).abodes;
		abodes.insert(abodes.begin(), dispenser);
		if (Locator::townSystem::has_value())
		{
			Locator::townSystem::value().StructuresChanged(town);
		}
	}
}

/// How tall a model stands at a scale
float HeightOfMesh(entt::id_type mesh, float scale)
{
	auto& meshes = Locator::resources::value().GetMeshes();
	return meshes.Contains(mesh) ? meshes.Handle(mesh)->GetBoundingBox().Size().y * scale : 0.0f;
}
} // namespace

// Dispensers and one-shot miracles

entt::entity MagicSystem::CreateDispenser(glm::vec3 position, MagicType type, float yAngleRadians)
{
	if (!Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	position.y = _world.LandHeight({position.x, position.z});
	const auto entity = archetypes::SpellDispenserArchetype::Create(position, type, k_DispenserBuilding, yAngleRadians, 1.0f);
	auto& dispenser = EntityRegistry().Get<SpellDispenser>(entity);
	if (Locator::effectRequestSystem::has_value())
	{
		// Its swirl uses only this computer's random numbers: the swirl does not change the world. The swirl moves on in
		// the frames that draw the dispenser, not in the turns.
		dispenser.effect = Locator::effectRequestSystem::value().Start(
		    {.type = ParticleType::SpelldispenserVortex, .position = position, .synced = false, .step = EffectStep::Frame});
	}
	// Its first bubble comes once it has counted its period, as every later one does. A debug tool makes it, so it has
	// no town.
	return entity;
}

entt::entity MagicSystem::CreateScriptDispenser(glm::vec3 position, AbodeInfo building, float yAngleRadians, float scale,
                                                entt::entity town)
{
	if (!Locator::infoConstants::has_value() || building == AbodeInfo::None ||
	    static_cast<size_t>(building) >= Locator::infoConstants::value().abode.size())
	{
		return entt::null;
	}
	position.y = _world.LandHeight({position.x, position.z});
	const auto entity = archetypes::SpellDispenserArchetype::Create(position, MagicType::None, building, yAngleRadians, scale);
	auto& dispenser = EntityRegistry().Get<SpellDispenser>(entity);
	// Made off: it floats nothing until a script turns it on
	dispenser.timer.active = false;
	JoinTown(entity, DispenserTown(town));
	if (Locator::effectRequestSystem::has_value())
	{
		// Its swirl uses only this computer's random numbers: the swirl does not change the world. The swirl moves on in
		// the frames that draw the dispenser, not in the turns.
		dispenser.effect = Locator::effectRequestSystem::value().Start(
		    {.type = ParticleType::SpelldispenserVortex, .position = position, .synced = false, .step = EffectStep::Frame});
	}
	// It starts to work at once, built or not. After the first turn, it gets its footpath from its town's storage pit.
	if (Locator::footpathSystem::has_value())
	{
		Locator::footpathSystem::value().BuildingWorking(entity);
	}
	return entity;
}

entt::entity MagicSystem::CreateTownDispenser(glm::vec3 position, AbodeInfo building, float yAngleRadians, float scale,
                                              entt::entity town, float built)
{
	const auto entity = CreateScriptDispenser(position, building, yAngleRadians, scale, town);
	// It does not work until all of it is built
	if (entity != entt::null && built < 1.0f)
	{
		EntityRegistry().Assign<BuildProgress>(entity, built < 0.0f ? 0.0f : built);
	}
	return entity;
}

void MagicSystem::SetDispenserMagic(entt::entity dispenser, MagicType type, uint32_t turns)
{
	auto* component = EntityRegistry().TryGet<SpellDispenser>(dispenser);
	if (component == nullptr)
	{
		return;
	}
	component->magicType = type;
	component->timer.period = turns;
	if (turns == 0)
	{
		SetDispenserActive(dispenser, false);
	}
}

void MagicSystem::ChargeDispenser(entt::entity dispenser)
{
	auto& registry = EntityRegistry();
	if (const auto* component = registry.TryGet<const SpellDispenser>(dispenser);
	    component != nullptr && component->orb == entt::null && component->magicType != MagicType::None)
	{
		MakeOrb(dispenser);
	}
}

void MagicSystem::SetDispenserPeriod(entt::entity dispenser, float seconds)
{
	if (auto* component = EntityRegistry().TryGet<SpellDispenser>(dispenser))
	{
		component->timer.period = magic::PeriodTurns(seconds, TimeSystemInterface::k_TurnDuration);
		component->timer.active = component->timer.period != 0;
	}
}

void MagicSystem::SetDispenserActive(entt::entity dispenser, bool active)
{
	auto* component = EntityRegistry().TryGet<SpellDispenser>(dispenser);
	if (component == nullptr)
	{
		return;
	}
	component->timer.active = active;
	if (active)
	{
		MakeOrb(dispenser);
	}
}

void MagicSystem::SetDispenserTurns(entt::entity dispenser, uint32_t turns)
{
	if (auto* component = EntityRegistry().TryGet<SpellDispenser>(dispenser); component != nullptr && turns != 0)
	{
		component->timer.period = turns;
	}
}

entt::entity MagicSystem::CreateOneOffSeed(glm::vec3 position, SpellSeedType seed, int powerUp, float multiplier)
{
	return archetypes::OneOffSpellSeedArchetype::Create(position, seed, powerUp, multiplier);
}

entt::entity MagicSystem::CreateOneOffSeedFor(glm::vec3 position, MagicType type)
{
	const auto seed = magic::FindFirstSpellSeedForMagicType(Info(), type);
	if (!seed.has_value())
	{
		return entt::null;
	}
	const auto step = magic::GetPowerUpGesture(magic::GetSpellSeedInfo(Info(), *seed), type);
	const auto orb = CreateOneOffSeed(position, *seed, step.level, 1.0f);
	if (orb != entt::null)
	{
		EntityRegistry().Get<OneOffSpellSeed>(orb).magicType = type;
	}
	return orb;
}

glm::vec3 MagicSystem::DispenserOrbPosition(entt::entity dispenser) const
{
	const auto& registry = EntityRegistry();
	const auto& transform = registry.Get<const Transform>(dispenser);
	const auto& mesh = registry.Get<const Mesh>(dispenser);
	return magic::OrbPosition(transform.position, HeightOfMesh(mesh.id, transform.scale.y));
}

void MagicSystem::StoreHeldSeedIn(entt::entity dispenser)
{
	auto& registry = EntityRegistry();
	if (!Held().has_value() || !registry.Valid(*Held()))
	{
		return;
	}
	// The held miracle becomes a one-shot globe above the dispenser, of the same seed and power-up. The globe is not the
	// dispenser's own, so the dispenser continues to make its own globes as before.
	const auto seed = registry.Get<const SpellSeed>(*Held());
	const auto position = DispenserOrbPosition(dispenser);
	if (CreateOneOffSeed(position, seed.seedType, seed.powerUp, 1.0f) != entt::null &&
	    Locator::effectRequestSystem::has_value())
	{
		Locator::effectRequestSystem::value().Show({.type = SpotVisualType::ObjectAppear, .position = position});
	}
	// The seed goes, with the prayer power that charged it
	LetGoOfSeed(true);
	if (ActsOnOwnHand())
	{
		_lastHandResult = HandResult::Stored;
	}
}

entt::entity MagicSystem::MakeOrb(entt::entity entity)
{
	auto& registry = EntityRegistry();
	auto& dispenser = registry.Get<SpellDispenser>(entity);
	const auto position = DispenserOrbPosition(entity);
	const auto magicType = dispenser.magicType;
	const auto orb = CreateOneOffSeedFor(position, magicType);
	auto& after = registry.Get<SpellDispenser>(entity);
	after.orb = orb;
	after.orbPosition = position;
	if (orb != entt::null)
	{
		// A new bubble starts the count again
		after.timer.tick = 0;
		registry.Get<OneOffSpellSeed>(orb).dispenser = entity;
		Locator::effectRequestSystem::value().Show({.type = SpotVisualType::MagicObjectCreated, .position = position});
	}
	return orb;
}

void MagicSystem::ProcessDispensers()
{
	auto& registry = EntityRegistry();
	std::vector<entt::entity> making;
	registry.Each<SpellDispenser, const Transform>([&](entt::entity entity, SpellDispenser& dispenser,
	                                                   const Transform& transform) {
		// The bubble stays the dispenser's while it is in the world and touches the dispenser on the ground
		bool stillThere = false;
		if (dispenser.orb != entt::null && registry.Valid(dispenser.orb))
		{
			// The turn measures from where the world has the bubble, not from where the frames draw it
			const auto* orb = registry.TryGet<const Transform>(dispenser.orb);
			if (orb != nullptr && registry.AllOf<OneOffSpellSeed>(dispenser.orb))
			{
				stillThere =
				    magic::Touching(glm::vec2(transform.position.x, transform.position.z), world_objects::SizeOf(entity).radius,
				                    glm::vec2(orb->position.x, orb->position.z), world_objects::SizeOf(dispenser.orb).radius);
			}
		}
		const magic::DispenserTurn turn {
		    .hasOrb = dispenser.orb != entt::null,
		    .orbStillThere = stillThere,
		    .hasMagic = dispenser.magicType != MagicType::None,
		    .whole = construction::IsBuilt(entity) && world_objects::LifeOf(entity) >= 1.0f,
		};
		switch (magic::StepDispenser(dispenser.timer, turn))
		{
		case magic::DispenserStep::OrbTaken:
			dispenser.orb = entt::null;
			break;
		case magic::DispenserStep::MakeOrb:
			making.push_back(entity);
			break;
		case magic::DispenserStep::Wait:
			break;
		}
	});
	for (const auto entity : making)
	{
		MakeOrb(entity);
	}
}

bool MagicSystem::Remove(entt::entity entity)
{
	auto& registry = EntityRegistry();
	if (!registry.Valid(entity))
	{
		return false;
	}
	if (const auto* dispenser = registry.TryGet<const SpellDispenser>(entity))
	{
		if (Locator::effectRequestSystem::has_value() && dispenser->effect != k_NoEffectHandle)
		{
			Locator::effectRequestSystem::value().Remove(dispenser->effect);
		}
		DestroyOrb(dispenser->orb);
		registry.Destroy(entity);
		return true;
	}
	if (registry.AllOf<OneOffSpellSeed>(entity))
	{
		DestroyOrb(entity);
		return true;
	}
	return false;
}

void MagicSystem::DestroyOrb(entt::entity orb)
{
	auto& registry = EntityRegistry();
	if (!registry.Valid(orb))
	{
		return;
	}
	registry.Destroy(orb);
}
