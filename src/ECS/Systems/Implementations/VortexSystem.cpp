/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VortexSystem.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <glm/gtc/constants.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Archetypes/VortexArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/LandFollowers.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/ScriptFind.h"
#include "ECS/ScriptFlocks.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/CreatureFizzSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerDisciples.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerProfileSystemInterface.h"
#include "ECS/Systems/ScriptObjectsSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TornadoSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/VortexCrossingLines.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "LHScriptX/Script.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "Magic/VortexCrossingRules.h"
#include "Magic/VortexPullRules.h"
#include "Magic/VortexRules.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleSpellLink.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using openblack::ecs::components::CarriedByTornado;
using openblack::ecs::components::Creature;
using openblack::ecs::components::DrawnIntoVortex;
using openblack::ecs::components::InPhysics;
using openblack::ecs::components::Transform;
using openblack::ecs::components::Vortex;

namespace
{
/// A vortex steps the effect drawing things in once a game turn
constexpr float k_TurnSeconds = std::chrono::duration<float>(TimeSystemInterface::k_TurnDuration).count();
/// The crossing file, in the player's saved games
constexpr std::string_view k_CrossingFile = "vortex.txt";

/// The map's cells run 0 to 511 along each side
bool OnMap(glm::ivec2 cell)
{
	return cell.x >= 0 && cell.y >= 0 && cell.x < LandIslandInterface::k_MapCellsPerSide &&
	       cell.y < LandIslandInterface::k_MapCellsPerSide;
}

/// Seconds since a vortex's state began, now
double SecondsInState(const Vortex& vortex)
{
	const auto& time = Locator::time::value();
	const auto turn = time.GetTurn();
	const auto turns = turn >= vortex.stateStartTurn ? turn - vortex.stateStartTurn : 0;
	return vortex::ElapsedSeconds(turns, time.GetTurnFraction(),
	                              static_cast<uint32_t>(TimeSystemInterface::k_TurnDuration.count()));
}

const GVortexInfo* InfoOf(VortexType type)
{
	const auto& tables = Locator::infoConstants::value().vortex;
	const auto row = static_cast<size_t>(type);
	return row < tables.size() ? &tables.at(row) : nullptr;
}

/// The vortex's swirl and its effect over the land are stepped by the vortex itself; no miracle hears from them
class NoMiracle final: public particles::SpellSink
{
public:
	bool SpellEvent(const particles::SpellEventInfo& /*event*/) override { return false; }
	[[nodiscard]] int PowerUpLevel() const override { return -1; }
};
NoMiracle g_noMiracle;
} // namespace

entt::entity VortexSystem::Create(glm::vec3 position, VortexType type, float altitude)
{
	const auto* info = InfoOf(type);
	if (info == nullptr)
	{
		return entt::null;
	}
	const auto ground = Locator::terrainSystem::value().GetHeightAt({position.x, position.z});
	const glm::vec3 centre {position.x, ground + altitude, position.z};
	const auto entity = archetypes::VortexArchetype::Create(centre, type, info->initialState, Locator::time::value().GetTurn());
	auto& vortex = Locator::entitiesRegistry::value().Get<Vortex>(entity);
	vortex.altitude = altitude;
	// Until a script says otherwise, the animals that come out gather at the vortex itself
	vortex.arrival.gatheringPlace = centre;
	StartEffects(vortex);
	if (type == VortexType::In)
	{
		StartCrossingFile(entity);
	}
	else if (type == VortexType::Out)
	{
		ReadCrossingFile(vortex);
	}
	return entity;
}

void VortexSystem::StartEffects(Vortex& vortex)
{
	if (!Locator::particleSystem::has_value())
	{
		return;
	}
	const auto* info = InfoOf(vortex.type);
	auto& particles = Locator::particleSystem::value();
	// The swirl starts on the ground under the middle, the others at the middle itself
	const auto ground = Locator::terrainSystem::value().GetHeightAt({vortex.centre.x, vortex.centre.z});
	const glm::vec3 onGround {vortex.centre.x, ground, vortex.centre.z};
	const auto start = [&particles](ParticleType type, glm::vec3 origin) {
		return type == ParticleType::None ? ParticleSystemInterface::k_NoEffect : particles.Start(type, origin, 1.0f);
	};
	const auto startStepped = [&particles](ParticleType type, glm::vec3 origin) {
		return type == ParticleType::None ? ParticleSystemInterface::k_NoEffect
		                                  : particles.StartForSpell(type, origin, glm::vec3(0.0f), 1.0f, g_noMiracle);
	};
	// The vortex steps the effect that draws things in itself, each turn just after it has handed it what it takes
	vortex.objectMoverEffect = startStepped(info->particleTypeObjectMover, vortex.centre);
	vortex.beforeLandEffect = startStepped(info->particleTypePreLandscape, onGround);
	vortex.afterLandEffect = startStepped(info->particleTypePostLandscape, vortex.centre);
	vortex.lightMapEffect = start(info->particleTypeLightMap, vortex.centre);
	// The swirl is drawn before the land, which covers it but where the vortex opens its hole
	if (vortex.beforeLandEffect != ParticleSystemInterface::k_NoEffect)
	{
		particles.SetDrawPath(vortex.beforeLandEffect, particles::draw::DrawPath::BeforeLand);
	}
}

void VortexSystem::DeleteEffects(const Vortex& vortex)
{
	if (!Locator::particleSystem::has_value())
	{
		return;
	}
	auto& particles = Locator::particleSystem::value();
	for (const auto effect : {vortex.objectMoverEffect, vortex.beforeLandEffect, vortex.afterLandEffect, vortex.lightMapEffect})
	{
		if (effect != ParticleSystemInterface::k_NoEffect)
		{
			particles.Delete(effect);
		}
	}
}

void VortexSystem::UpdateFrame(float gameSeconds)
{
	_groundMarks.clear();
	auto& registry = Locator::entitiesRegistry::value();
	auto* particles = Locator::particleSystem::has_value() ? &Locator::particleSystem::value() : nullptr;
	registry.Each<Vortex>([this, gameSeconds, particles](entt::entity /*entity*/, Vortex& vortex) {
		const auto seconds = static_cast<float>(SecondsInState(vortex));
		const auto openness = vortex::Openness(vortex.state, seconds);
		if (openness != 0.0f)
		{
			if (const auto* info = InfoOf(vortex.type); info != nullptr)
			{
				const auto cell = vortex::CentreCell({vortex.centre.x, vortex.centre.z});
				_groundMarks.push_back({
				    .type = vortex.type,
				    .centre = {vortex.centre.x, vortex.centre.z},
				    .block = {cell.x >> 4, cell.y >> 4},
				    .baseScale = info->baseScale,
				    .holeThreshold = vortex::GroundHoleThreshold(openness),
				});
			}
		}
		if (particles == nullptr)
		{
			return;
		}
		// The swirl and the effect over the land grow with the openness, stepped by the frame's game time; the swirl
		// rises towards the ground as the land is levelled
		const particles::ProcessInfo info {.power = openness, .enabled = true};
		if (openness != 0.0f && vortex.beforeLandEffect != ParticleSystemInterface::k_NoEffect)
		{
			const auto ground = Locator::terrainSystem::value().GetHeightAt({vortex.centre.x, vortex.centre.z});
			const auto depth = vortex::SwirlDepth(vortex::LevelAmount(vortex.state, seconds));
			particles->SetOrigin(vortex.beforeLandEffect, {vortex.centre.x, ground - depth, vortex.centre.z});
			if (!particles->ProcessForSpell(vortex.beforeLandEffect, info, gameSeconds))
			{
				vortex.beforeLandEffect = ParticleSystemInterface::k_NoEffect;
			}
		}
		if (openness != 0.0f && vortex.afterLandEffect != ParticleSystemInterface::k_NoEffect &&
		    !particles->ProcessForSpell(vortex.afterLandEffect, info, gameSeconds))
		{
			vortex.afterLandEffect = ParticleSystemInterface::k_NoEffect;
		}
		// The glow on the ground is as bright as the vortex's state has it
		if (auto* lightMap = particles->Find(vortex.lightMapEffect))
		{
			const auto glow = static_cast<double>(vortex::GlowBrightness(vortex.state, seconds));
			lightMap->SetGlobalAlpha(static_cast<float>(static_cast<int>(glow * 255.0)));
		}
	});
}

bool VortexSystem::StartFadeOut(entt::entity vortex)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(vortex))
	{
		return false;
	}
	auto* component = registry.TryGet<Vortex>(vortex);
	if (component == nullptr)
	{
		return false;
	}
	component->state = VortexStateType::FadeOut;
	component->stateStartTurn = Locator::time::value().GetTurn();
	return true;
}

bool VortexSystem::SetArrival(entt::entity vortex, const Vortex::Arrival& arrival)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(vortex))
	{
		return false;
	}
	auto* component = registry.TryGet<Vortex>(vortex);
	if (component == nullptr)
	{
		return false;
	}
	if (component->type == VortexType::Out)
	{
		component->arrival = arrival;
	}
	return true;
}

float VortexSystem::GetOpenness(entt::entity vortex) const
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(vortex))
	{
		return 0.0f;
	}
	const auto* component = registry.TryGet<Vortex>(vortex);
	if (component == nullptr)
	{
		return 0.0f;
	}
	return vortex::Openness(component->state, static_cast<float>(SecondsInState(*component)));
}

void VortexSystem::ProcessTurn()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> gone;
	bool levelled = false;
	std::vector<ecs::land_followers::Follower> followers;
	auto* effects = Locator::particleSystem::has_value() ? &Locator::particleSystem::value() : nullptr;
	registry.Each<Vortex>([&gone, &levelled, &followers, effects](entt::entity entity, Vortex& vortex) {
		// What is in it is dealt with first, as it stood before this turn's step of its fade
		if (vortex.type == VortexType::In && vortex.state == VortexStateType::Active)
		{
			Pull(entity, vortex);
		}
		else if (vortex.type == VortexType::Out)
		{
			ThrowOut(entity, vortex);
		}
		// Then its effect drawing things in steps, taking what it was just handed
		if (effects != nullptr && vortex.objectMoverEffect != ParticleSystemInterface::k_NoEffect &&
		    !effects->ProcessForSpell(vortex.objectMoverEffect, {.power = 1.0f, .enabled = true}, k_TurnSeconds))
		{
			vortex.objectMoverEffect = ParticleSystemInterface::k_NoEffect;
		}
		const auto seconds = SecondsInState(vortex);
		switch (vortex::Advance(vortex.state, seconds))
		{
		case vortex::Step::Open:
			vortex.state = VortexStateType::Active;
			break;
		case vortex::Step::Remove:
			vortex.state = VortexStateType::Inactive;
			gone.push_back(entity);
			return;
		case vortex::Step::Stay:
			break;
		}
		if (!vortex::LevelsGround(vortex.type))
		{
			return;
		}
		// The levelling only ever goes further
		const auto amount = vortex::LevelAmount(vortex.state, static_cast<float>(seconds));
		if (vortex.levelApplied < amount)
		{
			vortex.levelApplied = amount;
			// The living about it keep their height above the land as it is levelled under them
			const auto reach = static_cast<float>(vortex::k_LevelHalfSide + 1) * LandIslandInterface::k_CellSize;
			const glm::vec2 centre {vortex.centre.x, vortex.centre.z};
			// The static things about it don't move with the land, but keep the height above it they were put down at
			ecs::land_followers::KeepPlacedHeights(Locator::entitiesRegistry::value(), Locator::terrainSystem::value(),
			                                       centre - reach, centre + reach);
			for (const auto& follower : ecs::land_followers::Capture(
			         Locator::entitiesRegistry::value(), Locator::terrainSystem::value(), centre - reach, centre + reach))
			{
				// One about two vortices goes with the land under it once
				if (std::ranges::none_of(followers, [&follower](const auto& kept) { return kept.entity == follower.entity; }))
				{
					followers.push_back(follower);
				}
			}
			LevelGround(vortex, amount);
			levelled = true;
		}
	});
	for (const auto entity : gone)
	{
		DeleteEffects(registry.Get<Vortex>(entity));
		LetGoOfThrown(registry.Get<Vortex>(entity));
		registry.Destroy(entity);
		if (entity == _crossingVortex)
		{
			_crossing.close();
			_crossingVortex = entt::null;
		}
	}
	RemoveTheTaken();
	if (levelled)
	{
		Locator::terrainSystem::value().CommitAltitudeChanges();
		ecs::land_followers::Follow(registry, Locator::terrainSystem::value(), followers);
	}
}

void VortexSystem::Pull(entt::entity entity, const Vortex& vortex)
{
	if (!Locator::entitiesMap::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& map = Locator::entitiesMap::value();
	const auto* dynamics = Locator::dynamicsSystem::has_value() ? &Locator::dynamicsSystem::value() : nullptr;
	const auto centre = map_coords::FromMetres({vortex.centre.x, vortex.centre.z});
	const auto reach = vortex::PullReach(Locator::time::value().GetTurn(), vortex::k_PullRadius);
	for (const auto cell : vortex::CellsSearched(centre, reach))
	{
		// A copy: taking a pile may change the cell
		const auto things = map.GetAllInCell(cell);
		for (const auto thing : things)
		{
			const auto* transform = registry.Valid(thing) ? registry.TryGet<const Transform>(thing) : nullptr;
			// Only what stands in this cell; a thing over a cell's edge is met in its own
			if (transform == nullptr || map_coords::CellOf(transform->position) != cell)
			{
				continue;
			}
			// The creature is never drawn in: near the middle it sparkles away, if it may be taken
			if (const auto* creature = registry.TryGet<const Creature>(thing); creature != nullptr)
			{
				PullCreature(thing, *creature, transform->position, centre);
				continue;
			}
			// Whatever could be thrown is drawn in; buildings, features and the like stay
			if (dynamics != nullptr && dynamics->CanBecomePhysicsObject(thing))
			{
				HandOver(entity, vortex, thing);
			}
			else if (registry.AllOf<components::Pot>(thing))
			{
				TakeFromPile(entity, vortex, thing);
			}
		}
	}
}

void VortexSystem::HandOver(entt::entity vortexEntity, const Vortex& vortex, entt::entity thing,
                            const std::optional<DrawnIntoVortex::Flight>& flight)
{
	if (vortex.objectMoverEffect == ParticleSystemInterface::k_NoEffect || !Locator::particleSystem::has_value())
	{
		return;
	}
	Locator::entitiesRegistry::value().AssignOrReplace<DrawnIntoVortex>(
	    thing, DrawnIntoVortex {.vortex = vortexEntity, .flight = flight});
	Locator::particleSystem::value().AddTarget(vortex.objectMoverEffect, thing);
}

void VortexSystem::TakeFromPile(entt::entity vortexEntity, const Vortex& vortex, entt::entity pile)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& pot = registry.Get<const components::Pot>(pile);
	const auto& pots = Locator::infoConstants::value().pot;
	const auto row = static_cast<size_t>(pot.type);
	if (row >= pots.size() || (pots.at(row).potType != PotType::PileFood && pots.at(row).potType != PotType::PileWood) ||
	    !Locator::tornadoSystem::has_value())
	{
		return;
	}
	// A pot of up to so much, drawn at a share of its size
	const auto taken = Locator::tornadoSystem::value().TakeFromPile(pile, vortex::crossing::k_PileTake, 1.0f);
	if (taken == entt::null)
	{
		return;
	}
	const float random = Locator::gameRandom::has_value()
	                         ? Locator::gameRandom::value().GameFloatRand(vortex::crossing::k_PileShareRandom)
	                         : 0.0f;
	registry.Get<Transform>(taken).scale *= vortex::crossing::PileShare(random);
	HandOver(vortexEntity, vortex, taken);
}

std::optional<VortexSystemInterface::Taken> VortexSystem::EnterVortex(const std::shared_ptr<particles::CarriedObject>& carried)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (carried == nullptr || !registry.Valid(carried->object))
	{
		return std::nullopt;
	}
	const auto thing = carried->object;
	// The vortex it was handed by stays named on it while it is carried, as the thrower of a throw back out
	const auto* drawn = registry.TryGet<const DrawnIntoVortex>(thing);
	const entt::entity vortexEntity = drawn != nullptr ? drawn->vortex : entt::null;
	const auto flight = drawn != nullptr ? drawn->flight : std::nullopt;
	const auto* transform = registry.TryGet<const Transform>(thing);
	if (transform == nullptr || registry.AnyOf<CarriedByTornado>(thing))
	{
		registry.Remove<DrawnIntoVortex>(thing);
		return std::nullopt;
	}
	// A thing taken as it flew leaves the physics first, without going back onto the land; one that has stopped flying
	// since is not taken
	if (flight.has_value())
	{
		if (!registry.AllOf<InPhysics>(thing) || !Locator::dynamicsSystem::has_value())
		{
			registry.Remove<DrawnIntoVortex>(thing);
			return std::nullopt;
		}
		Locator::dynamicsSystem::value().RemoveObject(thing, false, false);
	}
	// It leaves the land as a thing starting to move does, without a body of its own: refused when it is moving already
	// or a script holds it where it is
	if (Locator::dynamicsSystem::has_value() &&
	    !Locator::dynamicsSystem::value().InitialisePhysics(thing, {.add = false}).started)
	{
		registry.Remove<DrawnIntoVortex>(thing);
		return std::nullopt;
	}
	// It starts where it stood, still, or where its body was as it flew, at the velocity it flew with
	const Taken taken {.position = flight.has_value() ? flight->position : transform->position,
	                   .rotation = flight.has_value() ? flight->rotation : transform->rotation,
	                   .velocity = flight.has_value() ? flight->velocity : glm::vec3(0.0f),
	                   .indestructible = registry.AllOf<components::Indestructible>(thing)};
	registry.Assign<CarriedByTornado>(thing, carried);
	const auto* vortex = registry.Valid(vortexEntity) ? registry.TryGet<const Vortex>(vortexEntity) : nullptr;
	if (taken.indestructible || vortex == nullptr)
	{
		return taken;
	}
	// It leaves the land for good: a villager its town, an animal its flock; then it is written down for the next land
	if (registry.AllOf<components::Villager>(thing) && Locator::townSystem::has_value())
	{
		Locator::townSystem::value().RemoveVillager(thing);
	}
	if (Locator::animalSystem::has_value())
	{
		Locator::animalSystem::value().LeaveFlock(thing);
	}
	if (vortexEntity == _crossingVortex)
	{
		WriteCrossing(*vortex, thing);
	}
	return taken;
}

void VortexSystem::StruckBy(entt::entity vortexEntity, entt::entity hitter, const DrawnIntoVortex::Flight& flight)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* vortex = registry.Valid(vortexEntity) ? registry.TryGet<const Vortex>(vortexEntity) : nullptr;
	if (vortex == nullptr || hitter == entt::null || !registry.Valid(hitter))
	{
		return;
	}
	// It takes what it would draw in from the land: a creature only as it would take one, anything else that can fly
	const auto* creature = registry.TryGet<const Creature>(hitter);
	const bool mayBeTaken = creature != nullptr ? CreatureMayBeTaken(hitter, *creature)
	                                            : Locator::dynamicsSystem::has_value() &&
	                                                  Locator::dynamicsSystem::value().CanBecomePhysicsObject(hitter);
	if (!vortex::TakesWhatStrikesIt(vortex->type, vortex->state, registry.AllOf<InPhysics>(hitter), mayBeTaken))
	{
		return;
	}
	if (creature != nullptr)
	{
		if (const auto* transform = registry.TryGet<const Transform>(hitter))
		{
			PullCreature(hitter, *creature, transform->position, map_coords::FromMetres({vortex->centre.x, vortex->centre.z}));
		}
		return;
	}
	HandOver(vortexEntity, *vortex, hitter, flight);
}

void VortexSystem::ThrowOutOfVortex(entt::entity object, float angle, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	// Back onto the land where its particle left it, then thrown
	if (const auto* carried = registry.TryGet<const CarriedByTornado>(object);
	    carried != nullptr && carried->carried != nullptr)
	{
		auto& transform = registry.Get<Transform>(object);
		transform.position = carried->carried->position;
		transform.rotation = carried->carried->rotation;
	}
	registry.Remove<CarriedByTornado>(object);
	if (Locator::dynamicsSystem::has_value())
	{
		Locator::dynamicsSystem::value().EndPhysicsAsObject(object, true, false);
	}
	// The vortex that took it in throws it
	entt::entity thrower = entt::null;
	if (const auto* drawn = registry.TryGet<const DrawnIntoVortex>(object))
	{
		thrower = registry.Valid(drawn->vortex) ? drawn->vortex : entt::null;
		registry.Remove<DrawnIntoVortex>(object);
	}
	Throw(object, angle, scale, thrower);
}

void VortexSystem::Throw(entt::entity object, float angle, float scale, entt::entity thrower)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!Locator::dynamicsSystem::has_value() || !Locator::gameRandom::has_value())
	{
		return;
	}
	float height = 0.0f;
	if (const auto* mesh = registry.TryGet<const components::Mesh>(object);
	    mesh != nullptr && Locator::resources::has_value() && Locator::resources::value().GetMeshes().Contains(mesh->id))
	{
		// Twice its model's reach above its origin, scaled
		height = Locator::resources::value().GetMeshes().Handle(mesh->id)->GetBoundingBox().maxima.y * 2.0f *
		         registry.Get<const Transform>(object).scale.x;
	}
	auto& random = Locator::gameRandom::value();
	const float out = random.GameFloatRand(vortex::crossing::k_ThrowOutRandom);
	const float up = random.GameFloatRand(vortex::crossing::k_ThrowOutRandom);
	const auto throwOut = vortex::crossing::ThrowOutOf(angle, out, up, scale, height);
	Locator::dynamicsSystem::value().InitialisePhysics(
	    object, {.velocity = throwOut.velocity, .spin = throwOut.spin, .thrower = thrower});
}

void VortexSystem::StartCrossingFile(entt::entity vortex)
{
	_crossing.close();
	_crossingVortex = entt::null;
	_written.clear();
	if (!Locator::playerProfileSystem::has_value())
	{
		return;
	}
	const auto folder = Locator::playerProfileSystem::value().SavedGamesFolder();
	if (!folder.has_value())
	{
		return;
	}
	std::error_code error;
	std::filesystem::create_directories(*folder, error);
	// Emptied by every new vortex that leads away
	_crossing.open(*folder / k_CrossingFile, std::ios::out | std::ios::trunc | std::ios::binary);
	if (_crossing.is_open())
	{
		_crossingVortex = vortex;
	}
	else if (const auto logger = spdlog::get("game"); logger != nullptr)
	{
		logger->warn("Vortex: could not write the crossing file in {}", folder->string());
	}
}

void VortexSystem::WriteCrossing(const Vortex& vortex, entt::entity thing)
{
	// Each thing once
	if (!_crossing.is_open() || !_written.insert(thing).second)
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& land = Locator::terrainSystem::value();
	const auto& info = Locator::infoConstants::value();
	const ecs::vortex_crossing::World world {
	    .turn = Locator::time::value().GetTurn(),
	    .ground = [&land](glm::vec2 xz) { return land.GetHeightAt(xz); },
	    .villagerRow = [](Tribe tribe,
	                      VillagerNumber number) { return static_cast<int32_t>(GVillagerInfo::Find(tribe, number)); },
	    .potResource =
	        [&info](PotInfo type) {
		        const auto row = static_cast<size_t>(type);
		        return row < info.pot.size() ? static_cast<int32_t>(info.pot.at(row).resourceType) : 0;
	        },
	    .life = [](entt::entity object) { return world_objects::LifeOf(object); },
	    .player = [](entt::entity object) { return world_objects::PlayerOf(object); },
	    .magicName =
	        [&info](SpellSeedType seed, int powerUp) {
		        const auto& record = magic::GetMagicInfoFromPowerUpLevel(info, magic::GetSpellSeedInfo(info, seed), powerUp);
		        // The name fills its field up to the first nought, if it has one
		        const auto& name = magic::GetMagicEffectInfo(info, record.magicType).debugString;
		        return std::string(name.begin(), std::ranges::find(name, '\0'));
	        },
	    .filedAsStatic =
	        [](entt::entity object) {
		        const auto* objectInfo = world_objects::InfoOf(object);
		        return objectInfo != nullptr && objectInfo->type == ObjectType::MobileStatic;
	        },
	};
	auto place = map_coords::FromMetres({vortex.centre.x, vortex.centre.z});
	place.altitude = vortex.altitude;
	if (const auto line = ecs::vortex_crossing::LineFor(registry, thing, place, world))
	{
		const auto text = vortex::crossing::Text(*line);
		_crossing.write(text.data(), static_cast<std::streamsize>(text.size()));
		_crossing.flush();
		if (std::holds_alternative<vortex::crossing::MobileStaticLine>(*line))
		{
			for (const auto identical : ecs::vortex_crossing::IdenticalStatics(registry, thing, world))
			{
				_written.insert(identical);
			}
		}
	}
}

void VortexSystem::ReadCrossingFile(Vortex& vortex)
{
	// The file is read from the start; none is as good as an empty one
	auto& progress = vortex.throwingOut;
	progress = {};
	Locator::entitiesRegistry::value().Context().mapScriptGlobals.lastObject = entt::null;
	if (!Locator::playerProfileSystem::has_value())
	{
		return;
	}
	const auto folder = Locator::playerProfileSystem::value().SavedGamesFolder();
	if (!folder.has_value())
	{
		return;
	}
	std::ifstream file(*folder / k_CrossingFile, std::ios::binary);
	for (std::string line; std::getline(file, line);)
	{
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}
		progress.lines.push_back(std::move(line));
	}
}

void VortexSystem::ThrowOut(entt::entity entity, Vortex& vortex)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& progress = vortex.throwingOut;
	auto& arrival = vortex.arrival;
	// Flocks and a town that have gone are forgotten; with no town it takes the nearest
	if (!registry.Valid(progress.animalFlock) || !registry.AllOf<components::Flock>(progress.animalFlock))
	{
		progress.animalFlock = entt::null;
	}
	if (!registry.Valid(arrival.flock) || !ecs::script_flocks::IsFlock(registry, arrival.flock))
	{
		arrival.flock = entt::null;
	}
	if (!registry.Valid(arrival.town) || !registry.AllOf<components::Town>(arrival.town))
	{
		arrival.town =
		    NearestTownWithin(map_coords::FromMetres({vortex.centre.x, vortex.centre.z}), vortex::crossing::k_ArrivalTownReach);
	}
	// What it threw that has gone leaves its keeping; the villagers it still keeps stay at full life
	std::erase_if(progress.thrown, [&registry](entt::entity thing) { return !registry.Valid(thing); });
	for (const auto thing : progress.thrown)
	{
		if (registry.AllOf<components::Villager>(thing))
		{
			world_objects::SetLife(thing, 1.0f);
		}
	}
	// It doesn't start to fade out until the file is read and all its villagers are out
	const auto turn = Locator::time::value().GetTurn();
	if (vortex::crossing::HoldsFade(progress.reading, progress.villagersOut))
	{
		vortex.stateStartTurn = turn;
	}
	auto& lastObject = registry.Context().mapScriptGlobals.lastObject;
	if (!registry.Valid(lastObject))
	{
		lastObject = entt::null;
	}
	// Every second turn something comes out at a random angle
	if (turn % 2 != 0 || !Locator::gameRandom::has_value())
	{
		return;
	}
	auto& random = Locator::gameRandom::value();
	const float angle = random.GameFloatRand(glm::two_pi<float>());
	auto vortexPlace = map_coords::FromMetres({vortex.centre.x, vortex.centre.z});
	vortexPlace.altitude = vortex.altitude;
	const auto place = vortex::crossing::ThrowOutPlace(vortexPlace, angle);
	const auto placeMetres = map_coords::ToMetres(place);
	if (progress.reading)
	{
		// The file's next line makes its thing again about the place; at its end the reading stops
		if (progress.nextLine >= progress.lines.size())
		{
			progress.reading = false;
			progress.lines.clear();
			return;
		}
		const auto& line = progress.lines.at(progress.nextLine++);
		try
		{
			lhscriptx::Script script(glm::vec3(placeMetres.x, place.altitude, placeMetres.y));
			script.Load(line);
		}
		catch (const std::runtime_error& error)
		{
			if (const auto logger = spdlog::get("game"); logger != nullptr)
			{
				logger->warn("Vortex: the crossing line \"{}\" failed: {}", line, error.what());
			}
		}
		// The thing the line made or, when it made none, the last thing made before it that is still there
		const auto thing = lastObject;
		if (!registry.Valid(thing))
		{
			return;
		}
		if (registry.AllOf<components::Villager>(thing))
		{
			++progress.villagersOut;
		}
		JoinAndThrow(entity, vortex, thing, angle);
		return;
	}
	if (progress.villagersOut >= vortex::crossing::k_ArrivalVillagers)
	{
		return;
	}
	// Then new villagers of its town's tribe, every fifth a child
	const auto count = ++progress.villagersOut;
	const auto* tribe = registry.Valid(arrival.town) ? registry.TryGet<const Tribe>(arrival.town) : nullptr;
	const auto tribeNumber = tribe != nullptr ? static_cast<int32_t>(*tribe) : vortex::crossing::k_ArrivalTribeWithoutTown;
	const float housewife = random.GameFloatRand(1.0f);
	const uint32_t job = housewife < vortex::crossing::k_HousewifeShare ? 0 : random.GameRand(vortex::crossing::k_JobRandom);
	const auto row = vortex::crossing::NewVillagerRow(tribeNumber, housewife, job);
	const auto age = vortex::crossing::NewVillagerAge(count, random.GameRand(vortex::crossing::NewVillagerAgeLimit(count)));
	if (row < 0 || std::cmp_greater_equal(row, Locator::infoConstants::value().villager.size()))
	{
		return;
	}
	const glm::vec3 position {placeMetres.x, Locator::terrainSystem::value().GetHeightAt(placeMetres) + place.altitude,
	                          placeMetres.y};
	const auto villager = archetypes::VillagerArchetype::Create(position, position, static_cast<VillagerInfo>(row), age,
	                                                            archetypes::VillagerArchetype::Joins::Nobody);
	if (villager != entt::null)
	{
		JoinAndThrow(entity, vortex, villager, angle);
	}
}

void VortexSystem::JoinAndThrow(entt::entity entity, Vortex& vortex, entt::entity thing, float angle)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& progress = vortex.throwingOut;
	const auto& arrival = vortex.arrival;
	// Never twice
	if (std::ranges::find(progress.thrown, thing) != progress.thrown.end())
	{
		return;
	}
	if (const auto* person = registry.TryGet<const components::Villager>(thing))
	{
		// A villager joins the vortex's town, leaving its own; given a flock, it comes as a disciple from the vortex
		if (arrival.town != entt::null && Locator::townSystem::has_value())
		{
			auto& towns = Locator::townSystem::value();
			if (registry.Valid(person->town))
			{
				towns.RemoveVillager(thing);
			}
			towns.AddVillagerToTown(arrival.town, thing);
			if (arrival.flock != entt::null)
			{
				villager_disciple::SetType(thing, VillagerDisciple::FromVortex);
			}
		}
		// It joins the flock, and a flock a script holds hands it to the script, which takes control of it
		if (arrival.flock != entt::null)
		{
			ecs::script_flocks::AddLiving(registry, arrival.flock, thing);
			if (registry.AllOf<components::InScript>(arrival.flock) && Locator::scriptObjects::has_value())
			{
				Locator::scriptObjects::value().AddReference(thing);
				registry.AssignOrReplace<components::ScriptControlled>(thing);
			}
		}
	}
	bool dove = false;
	if (const auto* animal = registry.TryGet<const components::Animal>(thing);
	    animal != nullptr && Locator::animalSystem::has_value())
	{
		// An animal joins the flock gathering where the script said: a new one when the flock's leader is of another kind
		auto& animals = Locator::animalSystem::value();
		const auto kind = animal->type;
		const auto leader = progress.animalFlock != entt::null ? animals.LeaderOf(progress.animalFlock) : entt::null;
		const auto* leading = registry.Valid(leader) ? registry.TryGet<const components::Animal>(leader) : nullptr;
		if (progress.animalFlock == entt::null || (leading != nullptr && leading->type != kind))
		{
			progress.animalFlock = animals.CreateFlock({arrival.gatheringPlace.x, arrival.gatheringPlace.z},
			                                           arrival.flockRadius, arrival.flockDistance);
		}
		animals.JoinFlock(thing, progress.animalFlock);
		registry.Get<components::Animal>(thing).town = arrival.town;
		dove = kind == AnimalInfo::Dove || kind == AnimalInfo::SpellDove;
	}
	// Doves fly off on their own; everything else is kept and thrown out
	if (dove)
	{
		return;
	}
	progress.thrown.push_back(thing);
	Throw(thing, angle, 1.0f, entity);
	registry.AssignOrReplace<components::Indestructible>(thing);
}

bool VortexSystem::IsThrownOutByAVortex(entt::entity thing) const
{
	// Only the vortices into the land keep what they threw; the others never throw anything out
	bool kept = false;
	Locator::entitiesRegistry::value().Each<const Vortex>([thing, &kept](const Vortex& vortex) {
		kept = kept || vortex::crossing::KeepsThrown(vortex.type, vortex.throwingOut.thrown, thing);
	});
	return kept;
}

void VortexSystem::LetGoOfThrown(const Vortex& vortex)
{
	// What it threw out can be destroyed again once it has gone
	auto& registry = Locator::entitiesRegistry::value();
	for (const auto thing : vortex.throwingOut.thrown)
	{
		if (registry.Valid(thing))
		{
			registry.Remove<components::Indestructible>(thing);
		}
	}
}

entt::entity VortexSystem::NearestTownWithin(const map_coords::MapCoords& place, float reach)
{
	// Each player's towns in turn and the neutral ones last, the first met winning a tie
	const auto& registry = Locator::entitiesRegistry::value();
	struct Entry
	{
		PlayerNames owner;
		uint32_t id;
		ecs::script_find::Candidate candidate;
	};
	std::vector<Entry> towns;
	registry.Each<const components::Town, const Transform>(
	    [&towns](entt::entity town, const components::Town& data, const Transform& transform) {
		    towns.push_back(
		        {.owner = data.owner,
		         .id = data.id,
		         .candidate = {.entity = town, .at = map_coords::FromMetres({transform.position.x, transform.position.z})}});
	    });
	std::ranges::sort(towns, [](const Entry& a, const Entry& b) { return std::tie(a.owner, a.id) < std::tie(b.owner, b.id); });
	std::vector<ecs::script_find::Candidate> ordered;
	ordered.reserve(towns.size());
	std::ranges::transform(towns, std::back_inserter(ordered), &Entry::candidate);
	return ecs::script_find::FindNearestTown(place, reach, ordered);
}

void VortexSystem::RemoveTheTaken()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> taken;
	registry.Each<const CarriedByTornado>([&taken](entt::entity entity, const CarriedByTornado& carried) {
		if (carried.carried != nullptr && carried.carried->carrier == particles::CarriedObject::Carrier::Vortex &&
		    carried.carried->atom == nullptr)
		{
			taken.push_back(entity);
		}
	});
	for (const auto entity : taken)
	{
		registry.Remove<CarriedByTornado>(entity);
		world_objects::Remove(entity);
	}
}

bool VortexSystem::CreatureMayBeTaken(entt::entity entity, const Creature& creature)
{
	// Taken when a script let it through and it is on the leash, or whenever a script controls it
	const bool leashed = Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(entity);
	const bool scripted = Locator::entitiesRegistry::value().AllOf<components::ScriptControlled>(entity);
	return vortex::CreatureMayBeTaken(creature.mayGoThroughVortex, leashed, scripted);
}

void VortexSystem::PullCreature(entt::entity entity, const Creature& creature, const glm::vec3& position,
                                const map_coords::MapCoords& centre)
{
	if (!CreatureMayBeTaken(entity, creature) || !Locator::creatureFizzSystem::has_value())
	{
		return;
	}
	// Near enough the middle it sparkles away for good: it crosses in its own files when the land is left
	const auto at = map_coords::FromMetres({position.x, position.z});
	if (gutils::GetDistanceInMetres(at, centre) < CreatureTakenDistance())
	{
		Locator::creatureFizzSystem::value().SetFizz(entity, 1.0f, vortex::k_CreatureSparkleSeconds, true);
	}
}

float VortexSystem::CreatureTakenDistance()
{
	// The vortex's own model, unscaled
	const auto meshId = resources::HashIdentifier(MeshId::SpellVortexCylinder);
	if (!Locator::resources::has_value() || !Locator::resources::value().GetMeshes().Contains(meshId))
	{
		return 0.0f;
	}
	const auto box = Locator::resources::value().GetMeshes().Handle(meshId)->GetBoundingBox();
	return vortex::CreatureTakenDistance(box.maxima.x, box.maxima.z, 1.0f);
}

void VortexSystem::LevelGround(Vortex& vortex, float amount)
{
	auto& island = Locator::terrainSystem::value();
	const glm::vec2 centre {vortex.centre.x, vortex.centre.z};
	const auto centreCell = vortex::CentreCell(centre);
	if (vortex.groundHeights.empty())
	{
		// The square's heights as they are now, cells off the land counting as 0
		vortex.groundHeights.resize(vortex::k_LevelCellCount);
		for (size_t i = 0; i < vortex.groundHeights.size(); ++i)
		{
			const auto cell = vortex::SquareCell(centreCell, i);
			const auto* landCell = OnMap(cell) ? island.FindCell(glm::u16vec2(cell)) : nullptr;
			vortex.groundHeights[i] = landCell != nullptr ? landCell->altitude : 0;
		}
		vortex.groundAverage = vortex::AverageAltitude(vortex.groundHeights);
	}
	const auto heights = vortex::LevelSquare(centre, vortex.groundHeights, vortex.groundAverage, amount);
	for (size_t i = 0; i < heights.size(); ++i)
	{
		const auto cell = vortex::SquareCell(centreCell, i);
		if (OnMap(cell))
		{
			island.SetCellAltitude(glm::u16vec2(cell), heights.at(i));
		}
	}
}
