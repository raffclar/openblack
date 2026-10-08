/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Vortex.h"

#include <cmath>

#include <algorithm>
#include <bit>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/VortexSave.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "LHScriptX/Script.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// What this module keeps between calls (Locator::scriptState)
struct VortexState
{
	/// the vortexes in creation order (the newest first, inferred from the list insert)
	std::vector<entt::entity> vortexes {};
};

VortexState& VortexData()
{
	return openblack::Locator::scriptState::value().Get<VortexState>();
}

LandscapeVortex* Find(entt::entity vortex)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(vortex) ? registry.TryGet<LandscapeVortex>(vortex) : nullptr;
}

/// The seconds in the state, 0 when Inactive
float StateSeconds(const LandscapeVortex& v)
{
	if (v.state == VortexStateType::Inactive)
	{
		return 0.0f;
	}
	// the zero-extended turn difference, + the turn fraction, x the ms per turn, x 0.001
	const auto turns = static_cast<float>(static_cast<int64_t>(game_clock::Turn() - v.stateTurn));
	const float t = turns + game_clock::TurnFraction();
	return t * static_cast<float>(game_clock::MsPerTurn()) * 0.001f;
}

/// LandFactorValue of the vortex
float LandFactor(const LandscapeVortex& v)
{
	return vortex::LandFactorValue(v.state, StateSeconds(v));
}

/// An In or Out pulls the 11 x 11 cells under it to their mean as it fades in
void FlattenLand(LandscapeVortex& v)
{
	if (v.type == VortexType::Volcano || !Locator::terrainSystem::has_value()) // not the volcano
	{
		return;
	}
	const float q = LandFactor(v);
	if (!(q > v.landQ))
	{
		return;
	}
	v.landQ = q;
	auto& island = Locator::terrainSystem::value();
	const int cx = static_cast<int>(v.position.x * 0.1f); // truncated toward zero
	const int cz = static_cast<int>(v.position.z * 0.1f);
	const int half = static_cast<int>(58.0f * 2.0f * 0.1f) / 2; // 11 cells, halved: 5
	const auto altitudeAt = [&island](int x, int z) -> uint8_t {
		if (x < 0 || x > 0x1FF || z < 0 || z > 0x1FF || !island.HasBlockAt(glm::u16vec2(x, z)))
		{
			return 0;
		}
		return static_cast<uint8_t>(island.GetCellAltitude(island.GetCell(glm::u16vec2(x, z))) & 0xFF);
	};
	if (v.savedAltitudes.empty())
	{
		float sum = 0.0f;
		for (int x = cx - half; x <= cx + half; ++x)
		{
			for (int z = cz - half; z <= cz + half; ++z)
			{
				v.savedAltitudes.push_back(altitudeAt(x, z));
				sum += static_cast<float>(v.savedAltitudes.back());
			}
		}
		v.meanAltitude = sum / static_cast<float>(v.savedAltitudes.size());
	}
	size_t i = 0;
	bool changed = false;
	for (int x = cx - half; x <= cx + half; ++x)
	{
		for (int z = cz - half; z <= cz + half; ++z)
		{
			const float alt0 = static_cast<float>(v.savedAltitudes.at(i++));
			const glm::vec2 d(static_cast<float>(x) * 10.0f - v.position.x, static_cast<float>(z) * 10.0f - v.position.z);
			const int alt =
			    std::clamp(static_cast<int>(std::nearbyint(vortex::LandOffset(d, alt0, v.meanAltitude, q) + alt0)), 0, 255);
			// the original writes every cell; openblack's island has none off the map (as CitadelArchetype)
			if (x >= 0 && z >= 0 && x <= 0x1FF && z <= 0x1FF && alt != altitudeAt(x, z))
			{
				island.SetCellAltitude(glm::u16vec2(x, z), static_cast<uint16_t>(alt));
				changed = true;
			}
		}
	}
	if (changed)
	{
		island.RebuildAltitudes(); // (openblack) the land follows the new altitudes, only when a cell changed
	}
}

/// An object can when it can become a physics object; only the creature overrides it (pending: the creature,
/// false here as there are no creatures)
bool CanBeSuckedIntoVortex(entt::entity object)
{
	if (Locator::entitiesRegistry::value().AllOf<Creature>(object))
	{
		return false;
	}
	return physics::PhysicsObjects::CanBecomeAPhysicsObject(object);
}

/// Get2DRadius x 0.7
float SuckRadius(entt::entity vortex)
{
	return object::Get2DRadius(vortex) * 0.7f;
}

/// The In's contents
void ProcessIn(entt::entity entity, LandscapeVortex& vortex, uint32_t turn)
{
	if (vortex.state != VortexStateType::Active)
	{
		return;
	}
	// the whole radius on every 3rd turn, half otherwise
	const float r = turn % 3 == 0 ? vortex.radius : vortex.radius * 0.5f;
	auto cell = map_coords::CellOf(vortex.position);
	glm::ivec2 offset(0); // the spiral's whole-cell offset (stepping the MapCoords adds only the high words)
	map_coords::Spiral spiral;
	for (int remaining = 9999; remaining > 0; --remaining) // 9999 cells
	{
		if (!map_coords::InBounds(cell))
		{
			// off the map, straight to the next spiral step
		}
		else
		{
			// A walked cell farther than R ends the walk. The walked MapCoords keeps the vortex's fractions, so the distance is
			// exactly the whole-cell offset's: Hypotenuse(dx << 16, dz << 16) in metres
			const float distance = gutils::ConvertWholeDistanceToMeters(
			    gutils::Hypotenuse(static_cast<int32_t>(static_cast<uint32_t>(offset.x) << 16),
			                       static_cast<int32_t>(static_cast<uint32_t>(offset.y) << 16)));
			if (distance > r)
			{
				return;
			}
			for (const auto object : map_cells::ObjectsInCell(cell))
			{
				// only in the cell of its own MapCoords, so an object over several cells is seen once
				if (map_coords::Cell(object::MapCoordsOf(object)) != cell)
				{
					continue;
				}
				if (CanBeSuckedIntoVortex(object))
				{
					const auto& transform = Locator::entitiesRegistry::value().Get<const Transform>(object);
					vortex::TakeIn(entity, object, glm::vec3(0.0f), transform.rotation, transform.position, false);
				}
				// (pending) else a pile (IsPileResource): 1000 of it into a new pile scaled (GameFloatRand(0.3) x 0.4 + 0.3) x
				// scale, taken the same way (the split lives in Storm.cpp's SplitPile today: to be shared first)
			}
		}
		const auto& step = spiral.Next();
		cell += glm::ivec2(step.x, step.z);
		offset += glm::ivec2(step.x, step.z);
	}
}

/// The Out hands an arriving object over
void HandOver(entt::entity entity, LandscapeVortex& vortex, entt::entity object, float angle)
{
	// already being thrown -> nothing
	if (std::ranges::find(vortex.thrownVillagers, object) != vortex.thrownVillagers.end())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* villager = registry.TryGet<Villager>(object); villager != nullptr && vortex.town != entt::null)
	{
		// out of its old town, into the Out's. (pending) with a flock too: its disciple setting (0, 12, 0)
		if (villager->town != entt::null && registry.Valid(villager->town))
		{
			town_villagers::RemoveVillager(villager->town, object);
		}
		town_villagers::AddVillagerToTown(vortex.town, object);
	}
	// (pending) a villager with a flock, town or not (outside the town branch): a flock call (unidentified) and, when the flock
	// is in a script, the script hand-over (the script takes control of it)
	// (pending) an Animal: the Out's animal flock (a new flock, its domain centre, the animal's flock)
	// A Dove (pending: whether the citadel and spell doves are doves too) ends here. Anything else goes to the front
	// of the thrown list, is flung (physics::particle_carried_objects::Fling, below) and gets a flag bit (pending: which
	// openblack flag that bit is)
	if (registry.AllOf<Animal>(object) && registry.Get<const Animal>(object).type == AnimalInfo::Dove)
	{
		return;
	}
	vortex.thrownVillagers.insert(vortex.thrownVillagers.begin(), object);
	physics::particle_carried_objects::Fling(object, entity, angle, 1.0f); // the player argument is not read
	                                                                       // (pending) the flag bit above
}

/// The Out's contents: the town, the thrown list, the emission on even turns (the file's objects, then 30 new
/// villagers) and the hand-over with its fling. (pending) the land script's last created object
void ProcessOut(entt::entity entity, LandscapeVortex& vortex, uint32_t turn)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (vortex.animalFlock != entt::null && !ecs::IsAvailable(vortex.animalFlock))
	{
		vortex.animalFlock = entt::null;
	}
	if (vortex.flock != entt::null && !ecs::IsAvailable(vortex.flock))
	{
		vortex.flock = entt::null;
	}
	if (vortex.town == entt::null) // the nearest town within 200 m of its own position
	{
		vortex.town = map_cells::GetNearestTown(map_coords::FromWorld(vortex.position), 200.0f);
	}
	// The thrown list: the gone ones leave it; a villager in it stays alive (its life set to 1.0)
	std::erase_if(vortex.thrownVillagers, [](entt::entity v) { return !ecs::IsAvailable(v); });
	for (const auto thrown : vortex.thrownVillagers)
	{
		if (registry.AllOf<Villager>(thrown))
		{
			life::SetLife(thrown, 1.0f);
		}
	}
	// while it still has something to emit (its file, or fewer than 30 new villagers) the state's turn follows the game
	// turn, so its fade does not run out while it emits
	if (vortex.reader != nullptr || vortex.statVillagers < 30)
	{
		vortex.stateTurn = game_clock::Turn();
	}
	// the land script's last created object is forgotten once it is not available
	if (const auto last = lhscriptx::Script::LastCreated();
	    last != entt::null && !physics::particle_carried_objects::IsAvailable(last))
	{
		lhscriptx::Script::SetLastCreated(entt::null);
	}
	(void)registry;
	(void)entity;
	if ((turn & 1) != 0) // only on even turns
	{
		return;
	}
	// the emission cell, 16 m away at a random angle; each axis' cell is (cell x 10 +- 16 trig) / 10 truncated toward zero, and
	// only the high word changes (the fraction of the vortex's own MapCoords is kept)
	const float angle = game_random::GameFloatRand(std::bit_cast<float>(0x40C90FDBu)); // 2 pi
	auto at = map_coords::FromWorld(vortex.position);
	const auto cellX = static_cast<float>(static_cast<uint32_t>(at.x) >> 16);
	const auto cellZ = static_cast<float>(static_cast<uint32_t>(at.z) >> 16);
	const float dx = static_cast<float>(std::sin(static_cast<double>(angle))) * 16.0f;  // sin unrounded, x 16
	const float dz = static_cast<float>(std::cos(static_cast<double>(angle))) * -16.0f; // cos, x -16
	const auto newX = static_cast<uint32_t>(static_cast<int32_t>((cellX * 10.0f + dx) / 10.0f)) & 0xFFFFu;
	const auto newZ = static_cast<uint32_t>(static_cast<int32_t>((cellZ * 10.0f + dz) / 10.0f)) & 0xFFFFu;
	at.x = static_cast<int32_t>((newX << 16) | (static_cast<uint32_t>(at.x) & 0xFFFFu));
	at.z = static_cast<int32_t>((newZ << 16) | (static_cast<uint32_t>(at.z) & 0xFFFFu));
	// With its VortexSave reader, the next line of vortex.txt run as a land-script command at `at` (every A position +=
	// at); the end of the file deletes the reader
	if (vortex.reader != nullptr)
	{
		const auto read = vortex_save::ReadNext(*vortex.reader, at);
		if (!read.has_value())
		{
			vortex.reader.reset();
			return;
		}
		// the created thing as an object; none -> nothing more this turn.
		// (approximate) an Object = a valid entity with a Transform
		const auto object = *read;
		if (object != entt::null && registry.Valid(object) && registry.AllOf<Transform>(object))
		{
			vortex.statObjects += 1;
			vortex.statVillagers += registry.AllOf<Villager>(object) ? 1 : 0; // IsVillager
			// (pending) the stat of its food and wood resources
			HandOver(entity, vortex, object, angle);
		}
		return;
	}
	// no file left: up to 30 new villagers, one per even turn
	if (vortex.statVillagers >= 30)
	{
		return;
	}
	vortex.statVillagers += 1;
	// the town's tribe, 7 without a town (Tribe::NORSE; inferred: the same enum, as TownDesire.h)
	auto tribe = Tribe::NORSE;
	if (vortex.town != entt::null)
	{
		if (const auto* t = registry.TryGet<const Tribe>(vortex.town))
		{
			tribe = *t;
		}
	}
	// GameFloatRand(1.0) < 0.5 -> the housewife (row tribe x 7), else GameRand(5) + 1 (forester .. leader)
	int row = static_cast<int>(tribe) * 7;
	if (!(game_random::GameFloatRand(1.0f) < 0.5f))
	{
		row += static_cast<int>(game_random::GameRand(5)) + 1;
	}
	// every 5th one (the new count % 5 == 0) is GameRand(9) + 1 years old, the others GameRand(6) + 16
	const uint32_t age = vortex.statVillagers % 5 == 0 ? game_random::GameRand(9) + 1 : game_random::GameRand(6) + 16;
	const auto position = map_coords::ToWorld(at);
	const auto villager = archetypes::VillagerArchetype::Create(position, position, static_cast<VillagerInfo>(row), age, false);
	if (villager != entt::null)
	{
		HandOver(entity, vortex, villager, angle);
	}
}
} // namespace

entt::entity vortex::Create(glm::vec3 position, VortexType type)
{
	auto& vortexState = VortexData();
	if (type != VortexType::In && type != VortexType::Out && type != VortexType::Volcano)
	{
		return entt::null;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	// The base: a static object with scale 1.0; the info row, a constant per class (In 0, Out 1, Volcano 2); the
	// radius, Create's 50.0
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	auto& vortex = registry.Assign<LandscapeVortex>(entity);
	vortex.type = type;
	vortex.info = static_cast<int32_t>(type);
	vortex.radius = 50.0f;
	vortex.position = position;
	if (type == VortexType::Out)
	{
		vortex.flockPosition = position; // the Out's flock position: its own
		// the vortex.txt reader, then the land script's last created object cleared
		vortex.reader = vortex_save::OpenReader();
		lhscriptx::Script::SetLastCreated(entt::null);
	}
	// The creation setup with the info.dat vortex row: the state. (pending) the mesh x baseScale, the particle systems
	// PreLandscape / PostLandscape / ObjectMover / LightMap (the engine's particle systems), the volcano's sound, the spline
	if (Locator::infoConstants::has_value())
	{
		const auto& info = Locator::infoConstants::value().vortex.at(static_cast<size_t>(type));
		vortex.state = info.initialState;
	}
	vortexState.vortexes.insert(vortexState.vortexes.begin(), entity);
	if (vortex.state == VortexStateType::FadeIn)
	{
		StartFadeIn(entity); // the fade's turn
	}
	return entity;
}

void vortex::StartFadeIn(entt::entity entity)
{
	if (auto* v = Find(entity))
	{
		v->state = VortexStateType::FadeIn;
		v->stateTurn = game_clock::Turn();
	}
}

void vortex::StartFadeOut(entt::entity entity)
{
	if (auto* v = Find(entity))
	{
		v->state = VortexStateType::FadeOut;
		v->stateTurn = game_clock::Turn();
	}
}

void vortex::SetParameters(entt::entity entity, entt::entity town, glm::vec3 position, float a, float b, entt::entity flock)
{
	// the town and the flock parameters are the Out's; (inferred) the others' do nothing
	if (auto* v = Find(entity); v != nullptr && v->type == VortexType::Out)
	{
		v->town = town;
		v->flockPosition = position;
		v->flockA = a;
		v->flockB = b;
		v->flock = flock;
	}
}

void vortex::ReactToPhysicsImpact(entt::entity entity, entt::entity hitter, glm::vec3 velocity, const glm::mat3& rows,
                                  glm::vec3 position)
{
	const auto* v = Find(entity);
	// active, a hitter, CanBeSuckedIntoVortex and IN_PHYSICS
	if (v == nullptr || v->type != VortexType::In || v->state != VortexStateType::Active || hitter == entt::null ||
	    !CanBeSuckedIntoVortex(hitter) || !physics::PhysicsObjects::IsFlying(hitter))
	{
		return;
	}
	TakeIn(entity, hitter, velocity, rows, position, true);
}

void vortex::TakeIn(entt::entity entity, entt::entity object, glm::vec3 velocity, const glm::mat3& rows, glm::vec3 position,
                    bool fromPhysics)
{
	auto* v = Find(entity);
	if (v == nullptr || !ecs::IsAvailable(object))
	{
		return;
	}
	// (pending) a creature: it fizzes (1.0, 3.0, 1) when closer than SuckRadius, creatures not ported
	(void)SuckRadius;
	// CanBecomeAPhysicsObject (the callers' CanBeSuckedIntoVortex / IN_PHYSICS) and the vortex's particle system:
	// without it (not created yet: the engine's particle systems, pending) nothing is taken
	if (v->psys[0] == 0)
	{
		return;
	}
	VortexObjectInfo info {
	    .object = object,
	    .fromPhysics = fromPhysics,
	    .velocity = velocity,
	    .rows = rows,
	    .position = position,
	    .vortex = entity,
	};
	// (pending) info.scriptHeld = the ScriptHeld flag bit
	v->queue.push_back(info);
}

void vortex::ProcessAll(uint32_t turn)
{
	auto& vortexState = VortexData();
	auto& registry = Locator::entitiesRegistry::value();
	std::erase_if(vortexState.vortexes, [&registry](entt::entity e) { return !registry.Valid(e); });
	const auto vortexes = vortexState.vortexes; // ToBeDeleted below
	for (const auto entity : vortexes)
	{
		auto* v = Find(entity);
		if (v == nullptr)
		{
			continue;
		}
		// First the contents (In / Out; the Volcano has none). (pending) the two particle systems' update
		if (v->type == VortexType::In)
		{
			ProcessIn(entity, *v, turn);
		}
		else if (v->type == VortexType::Out)
		{
			ProcessOut(entity, *v, turn);
		}
		const bool done = StateSeconds(*v) > 2.0f + 5.0f;
		if (v->state == VortexStateType::FadeIn && done)
		{
			v->state = VortexStateType::Active;
		}
		else if (v->state == VortexStateType::FadeOut && done)
		{
			v->state = VortexStateType::Inactive;
			OnDeleted(entity);
			ecs::ToBeDeleted(entity);
			continue;
		}
		FlattenLand(*v);
	}
}

namespace
{
float Smooth(float x)
{
	return (3.0f - (x + x)) * x * x; // ((3 - 2x) x) x
}
} // namespace

float vortex::FadeValue(VortexStateType state, float e)
{
	switch (state)
	{
	case VortexStateType::Inactive:
		return 0.0f;
	case VortexStateType::Active:
		return 1.0f;
	case VortexStateType::FadeIn:
		if (e < 2.0f)
		{
			return 0.0f;
		}
		return Smooth(std::clamp((e - 2.0f) / 5.0f, 0.0f, 1.0f));
	default: // FadeOut (any other state)
		return e < 5.0f ? Smooth(1.0f - e / 5.0f) : 0.0f;
	}
}

float vortex::LandFactorValue(VortexStateType state, float e)
{
	const float s = state == VortexStateType::FadeOut ? 1.0f : FadeValue(state, e);
	const float r = 1.0f - s;
	return 1.0f - r * r;
}

float vortex::LandOffset(glm::vec2 d, float alt0, float mean, float q)
{
	const float r = std::sqrt(d.x * d.x + d.y * d.y);
	float b = 0.0f;
	if (r <= 56.0f) // 58 - 2
	{
		b = r < 50.0f ? mean - alt0 : (56.0f - r) * (mean - alt0) / (56.0f - 50.0f);
	}
	return b * q; // (pending) the spline S(clamp(r, 0, 58)) when its y are not 0
}

bool vortex::IsVillagerBeingThrown(entt::entity villager)
{
	for (const auto entity : VortexData().vortexes)
	{
		// only the Out looks in its thrown list, the others say no
		if (const auto* v = Find(entity); v != nullptr && v->type == VortexType::Out &&
		                                  std::ranges::find(v->thrownVillagers, villager) != v->thrownVillagers.end())
		{
			return true;
		}
	}
	return false;
}

void vortex::OnDeleted(entt::entity entity)
{
	auto* v = Find(entity);
	if (v == nullptr)
	{
		return;
	}
	// In: its VortexSave writer goes; Out: its VortexSave reader and the thrown villagers' list
	v->saveFile = false;
	v->reader.reset();
	v->thrownVillagers.clear();
	// (pending) the 3D object and the four particle systems, through the engine
	std::erase(VortexData().vortexes, entity);
}
