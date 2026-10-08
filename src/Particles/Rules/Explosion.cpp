/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The beam explosion (MAGIC_TYPE 7-9 EXPLOSION_ONE*, seed BEAM_EXPLOSION; SF_BeamExplosionSingle / Many / Loads) and the
// rules of its spot visual SF_BeamExplosionFX. The spell is a plain Spell (SpellGeneral.cpp): everything it does comes
// from UR_Explosion's events. Wiki: docs/bw1-notes/magic.md, beam explosion.

#include "Explosion.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/AllMeshes.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsDistance.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/GroundMarks.h"
#include "ECS/Life.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PotResource.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/ScreenshotRequestSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerDeath.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Particles/PSys.h"
#include "Particles/PSysFile.h"
#include "Particles/PSysManager.h"
#include "Particles/PSysRegistry.h"
#include "Particles/PSysWaterRings.h"
#include "Particles/Rules/ExplodeObject.h"
#include "Particles/Rules/Shield.h"

using namespace openblack;
using namespace openblack::psys;

namespace
{
// ---- constants of the explosion ----
constexpr float k_DefaultRadius = 5.0f;      ///< The shield margin without a spell (else the spell's effect radius)
constexpr float k_ShieldRayHeight = 200.0f;  ///< The blast is tested from 200 m above its centre
constexpr float k_TargetShieldMargin = 2.0f; ///< A target inside a shield by 2 m is shielded
constexpr float k_CellMetres = 10.0f;        ///< A map cell
constexpr float k_SearchExtra = 20.0f;       ///< ceil((r + 20) / 10)^2 spiral cells
constexpr float k_TribalPowerMin = 1.0f;
constexpr float k_TribalPowerMax = 5.0f;
constexpr int k_SpotVisualBeamFx = 36;  ///< SPOT_VISUAL BEAM_EXPLOSION_FX
constexpr int k_BeamFxTurns = 60;       ///< 60 turns
constexpr int k_SpotVisualSmoke = 23;   ///< SMOKE on dry land
constexpr int k_SpotVisualSteam = 22;   ///< STEAM on water
constexpr float k_SmokeScale = 8.0f;    ///< The smoke's magnitude
constexpr float k_SmokeSeconds = 4.0f;  ///< 4 s worth of turns, & 0xFFFF
constexpr float k_ExplodeSpread = 6.0f; ///< The explode queue's spread (stored, unread)
constexpr bool k_DestroyByBeam = true;  ///< The objects are destroyed
constexpr int k_Rocks = 5;              ///< Five rocks
constexpr uint32_t k_RockMesh = 0x237;  ///< Pack mesh 567 (MSH_Z_SPELLROCK01), 0 past the pack
constexpr float k_RockSpread = 4.0f;    ///< PSysFloatRand(-4, 4) on x and z
constexpr float k_RockScale = 1.0f;
constexpr float k_RockScaleMin = 0.8f; ///< PSysFloatRand(0.8 x scale, 1.2 x scale)
constexpr float k_RockScaleMax = 1.2f;

/// UR_Explosion's per-collection data
struct CollectionData
{
	glm::vec3 centre {0.0f}; ///< The parent's position at the land's altitude, every step
	bool notStarted {true};  ///< InitCollection not run yet
	bool anyTarget {false};  ///< A target is still in the list
	float spread {0.0f};     ///< The ring of the blast, += SpreadSpeed x dt
	struct Target
	{
		unsigned int turn {0};            ///< the game turn it was last seen available
		entt::entity object {entt::null}; ///< NULL once handled
	};
	std::vector<Target> targets;
	int exploded {0}; ///< Up to MaxObjectsToExplode
	int deleted {0};  ///< Up to MaxObjectsToDelete
	bool smokeDone {false};
	bool beamDone {false};
	bool finished {false};            ///< A shield stopped the blast
	entt::entity beamFx {entt::null}; ///< The SF_BeamExplosionFX container
};

float LandHeight(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// (port guard) the spot visuals need the game's files and the registry: not there in the unit tests
bool CanCreateSpotVisuals()
{
	return Locator::filesystem::has_value() && Locator::entitiesRegistry::has_value();
}

bool Trace()
{
	return magic::TraceEnabled();
}

/// OPENBLACK_TEST_EXPLOSION_SHOT="<turns>,<path.png>[;...]": screenshots that many game turns after the first blast's
/// first step (the beam FX starts then; InitCollection 0.4 s later), like OPENBLACK_TEST_SHIELD_SHOT
struct Shot
{
	unsigned int turns;
	std::string path;
	bool done {false};
};

/// The explosion test hook's state, in the debug hooks' store (Locator::debugHooks)
struct ExplosionDebugHooksState
{
	bool blastStarted {false};
	unsigned int blastTurn {0};
	/// OPENBLACK_TEST_EXPLOSION_SHOT's screenshots, read at the first use
	std::optional<std::vector<Shot>> shots;
};

ExplosionDebugHooksState& ExplosionDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("explosion: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<ExplosionDebugHooksState>();
}

std::vector<Shot> ReadShots()
{
	std::vector<Shot> list;
	const char* value = std::getenv("OPENBLACK_TEST_EXPLOSION_SHOT");
	if (value == nullptr)
	{
		return list;
	}
	std::stringstream stream(value);
	std::string item;
	while (std::getline(stream, item, ';'))
	{
		if (const auto comma = item.find(','); comma != std::string::npos)
		{
			list.push_back({static_cast<unsigned int>(std::atoi(item.substr(0, comma).c_str())), item.substr(comma + 1)});
		}
	}
	return list;
}

std::vector<Shot>& Shots()
{
	auto& shots = ExplosionDebugHooksData().shots;
	if (!shots.has_value())
	{
		shots = ReadShots();
	}
	return *shots;
}

void TestShots()
{
	const auto& state = ExplosionDebugHooksData();
	if (!state.blastStarted || !Locator::screenshotRequest::has_value())
	{
		return;
	}
	const unsigned int turn = magic::CurrentTurn();
	for (auto& shot : Shots())
	{
		if (!shot.done && turn >= state.blastTurn + shot.turns)
		{
			shot.done = true;
			Locator::screenshotRequest::value().Request(shot.path);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Explosion test: screenshot {} turns after the blast (turn {}) -> {}",
			                   shot.turns, turn, shot.path);
			return;
		}
	}
}

bool IsAvailable(entt::entity object)
{
	return object != entt::null && ecs::fire::traits::IsAvailable(object);
}

/// The world position of a target: its MapCoords as a point (x, z from fixed point, the land's altitude + its height
/// above the land)
glm::vec3 PositionOf(entt::entity object)
{
	if (!Locator::entitiesRegistry::value().AllOf<ecs::components::Transform>(object))
	{
		return glm::vec3(0.0f);
	}
	return map_coords::ToWorld(ecs::object::MapCoordsOf(object));
}

/// UR_Explosion: MaxObjectsToDelete, MaxObjectsToExplode, MaxDistance, BlastSpeed, SpreadSpeed, TimeToDoEventsFor,
/// InitialDelay, SmokeDelay, BeamDelay. The property ranges are editor limits; the defaults are the original's
/// (20, 20, 100, 10, 10, 5, 3.5, 3 and 0; no file leaves one out but BeamDelay).
class Explosion final: public Modifier
{
public:
	explicit Explosion(const Object& object)
	    : maxObjectsToDelete(object.Int("MaxObjectsToDelete", 20))
	    , maxObjectsToExplode(object.Int("MaxObjectsToExplode", 20))
	    , maxDistance(object.Float("MaxDistance", 100.0f))
	    , blastSpeed(object.Float("BlastSpeed", 10.0f))
	    , spreadSpeed(object.Float("SpreadSpeed", 10.0f))
	    , timeToDoEventsFor(object.Float("TimeToDoEventsFor", 5.0f))
	    , initialDelay(object.Float("InitialDelay", 3.5f))
	    , smokeDelay(object.Float("SmokeDelay", 3.0f))
	    , beamDelay(object.Float("BeamDelay", 0.0f))
	{
	}

	/// One step of the collection
	bool ModifyCollection(Effect& effect, Collection& collection, Collection::Slot& /*slot*/) const override
	{
		auto& data = DataOf(this, collection);
		if (auto& hooks = ExplosionDebugHooksData(); !hooks.blastStarted)
		{
			hooks.blastStarted = true; // OPENBLACK_TEST_EXPLOSION_SHOT counts from the first blast's first step
			hooks.blastTurn = magic::CurrentTurn();
		}
		// The parent atom's position (or the origin), then y = the land's altitude there
		data.centre = collection.parent != nullptr ? collection.parent->position : effect.GetOrigin();
		data.centre.y = LandHeight(data.centre.x, data.centre.z);
		const float age = effect.CollectionAge(collection);
		if (!data.finished && !effect.Closing() && data.notStarted && age > initialDelay)
		{
			InitCollection(effect, data);
			data.notStarted = false;
		}
		if (!data.finished && !effect.Closing())
		{
			// the blast itself: a SpellEvent 2 at the centre every step for TimeToDoEventsFor after the delay (the spell's
			// default event: its burn / crush / hit in the effect radius)
			if (!data.notStarted && age < initialDelay + timeToDoEventsFor)
			{
				const SpellEventInfo event {.type = SpellEventInfo::Type::Point,
				                            .position = data.centre,
				                            .velocity = glm::vec3(0.0f),
				                            .strength = 1.0f,
				                            .checkShields = false,
				                            .target = entt::null};
				effect.SendSpellEvent(event);
			}
			if (!data.beamDone && age > beamDelay && CanCreateSpotVisuals())
			{
				// A BEAM_EXPLOSION_FX spot visual at the centre's MapCoords, magnitude 1, for 60 turns: the column and
				// cones. The effect starts at that MapCoords as a point: ToWorld of FromWorld
				const auto at = map_coords::ToWorld(map_coords::FromWorld(data.centre));
				data.beamFx = manager::CreateSpotVisualTurns(k_SpotVisualBeamFx, at, k_BeamFxTurns, entt::null, 1.0f);
				data.beamDone = true;
			}
			if (!data.smokeDone && age > smokeDelay && CanCreateSpotVisuals())
			{
				data.smokeDone = true;
				// Smoke on dry land, else steam; magnitude 8 for 4 s
				const int visual = ecs::pot_resource::IsDryLand(data.centre) ? k_SpotVisualSmoke : k_SpotVisualSteam;
				// The duration is the ticks of 4 s, the same as TicksForSeconds(4), then & 0xFFFF, passed as TURNS, at
				// the centre's MapCoords
				const auto smokeTurns =
				    static_cast<int>(static_cast<uint32_t>(game_clock::TicksForSeconds(k_SmokeSeconds)) & 0xFFFFu);
				const auto at = map_coords::ToWorld(map_coords::FromWorld(data.centre));
				manager::CreateSpotVisualTurns(visual, at, smokeTurns, entt::null, k_SmokeScale);
			}
		}
		else
		{
			// stopped or closing: the FX closes down, and the target list is emptied
			if (data.beamFx != entt::null && Locator::entitiesRegistry::has_value())
			{
				manager::CloseSpotVisual(data.beamFx);
				data.beamFx = entt::null;
			}
			data.targets.clear();
		}
		Update(effect, data);
		return true;
	}

	int maxObjectsToDelete, maxObjectsToExplode;
	float maxDistance, blastSpeed, spreadSpeed, timeToDoEventsFor, initialDelay, smokeDelay, beamDelay;

private:
	/// The CollectionData of this modifier in that collection (Collection::modifierData)
	static CollectionData& DataOf(const Modifier* self, Collection& collection)
	{
		return CollectionDataOf<CollectionData>(collection, self);
	}

	/// The blast's start: shields, rings or ground mark, the targets and the rocks
	void InitCollection(Effect& effect, CollectionData& data) const
	{
		const auto* sink = effect.GetSink();
		const entt::entity spell = sink != nullptr ? sink->SpellEntity() : entt::null;
		const bool hasSpell = spell != entt::null && Locator::entitiesRegistry::value().Valid(spell);
		// the margin: the spell's effect radius (BEAM 5, 5, 10), else 5
		const float margin = hasSpell ? magic::EffectInfoOf(spell).radius : k_DefaultRadius;
		// Inside a shield, the blast hits the shield: its point is where a ray from 200 m above meets the sphere, a
		// spark there, and a SpellEvent 4 at the centre with the shield's spell as the target. A 0 (the shield held)
		// stops the blast for good.
		if (const auto* sphere = shields::FindShieldContainingPoint(data.centre, margin); sphere != nullptr)
		{
			glm::vec3 impact = data.centre;
			shields::FindIntersect(*sphere, data.centre + glm::vec3(0.0f, k_ShieldRayHeight, 0.0f), data.centre, margin,
			                       impact);
			shields::AddImpactTarget(*sphere, impact);
			const SpellEventInfo event {.type = SpellEventInfo::Type::HitSpell,
			                            .position = data.centre,
			                            .velocity = glm::vec3(0.0f),
			                            .strength = 1.0f,
			                            .checkShields = false,
			                            .target = shields::SpellOf(*sphere)};
			if (effect.SendSpellEvent(event) == 0)
			{
				data.finished = true;
			}
			if (Trace())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Explosion: blast at ({:.1f}, {:.1f}) inside a shield, {}",
				                   data.centre.x, data.centre.z, data.finished ? "stopped" : "went through");
			}
		}
		data.targets.clear();
		if (data.finished || effect.Closing())
		{
			return;
		}
		// Three water rings (Particles/PSysWaterRings, the one implementation: growth 10 x 0.5, x 0.7 and x 1, angle 0,
		// aspect and rate 1), or on dry land (altitude >= 4) the crater (a pack mesh, not a sprite)
		if (!water_rings::AddExplosionRings(data.centre))
		{
			// A ground mark at the centre, turned PSysFloatRand(2 pi), size 8, that melts into the land and fades after
			// 15 s (ecs/GroundMarks.h; its smoke is not made)
			const float angle = effect.Random(6.28318548f);
			const auto mark = ecs::ground_marks::CreateExplosionMark(data.centre, angle);
			if (Trace())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Explosion: ground mark {} at ({:.1f}, {:.1f}), angle {:.2f}",
				                   static_cast<uint32_t>(mark), data.centre.x, data.centre.z, angle);
			}
		}
		// the targets: every available object of the ceil((r + 20) / 10)^2 cells of the spiral around the centre that is
		// counted in that cell (its own cell) and nearer than its Get2DRadius + r (the distance in metres from the
		// centre's MapCoords); r = MaxDistance x the tribal power (1..5)
		float r = maxDistance;
		if (hasSpell)
		{
			r *= std::clamp(magic::GetTribalPower(spell), k_TribalPowerMin, k_TribalPowerMax);
		}
		const int side = map_coords::FtoL(std::ceil((r + k_SearchExtra) / k_CellMetres));
		const int cells = side * side;
		const unsigned int turn = game_clock::Turn();
		{
			const auto& registry = Locator::entitiesRegistry::value();
			// the centre's MapCoords and its copy walked by the spiral from dir = count = 1, each cell checked in
			// bounds
			const auto centre = map_coords::FromMetres(glm::vec2(data.centre.x, data.centre.z));
			auto cell = centre;
			map_coords::Spiral spiral;
			for (int n = 0; n < cells; ++n)
			{
				if (map_coords::InBounds(cell))
				{
					// The fixed list first, then the mobile one, each from its head (ecs::map_cells)
					for (const auto object : ecs::map_cells::ObjectsInCell(map_coords::Cell(cell)))
					{
						if (!registry.Valid(object) || !IsAvailable(object))
						{
							continue;
						}
						// only in the cell of its own MapCoords
						const auto own = ecs::object::MapCoordsOf(object);
						if (!ecs::map_cells::IsOwnCell(own, map_coords::Cell(cell)))
						{
							continue;
						}
						const float reach = ecs::object::Get2DRadius(object) + r;
						if (gutils::GetDistanceInMetres(centre, own) < reach)
						{
							data.targets.push_back({turn, object});
							data.anyTarget = true;
						}
					}
				}
				map_coords::AddCells(cell, spiral.Next());
			}
		}
		data.spread = 0.0f;
		// Five rock meshes at centre + (rand(-4, 4), 0, rand(-4, 4)) (the z rand first), turned PSysFloatRand(2 pi)
		// about Y and scaled PSysFloatRand(0.8, 1.2) (drawn before the angle), queued to explode from centre - 5 m at
		// BlastSpeed, then deleted: only their pieces exist. The queue is emptied by UR_ExplodeObject
		// (Rules/ExplodeObject.cpp) at the end of this turn's game loop
		{
			const uint32_t rockMesh = k_RockMesh < static_cast<uint32_t>(MeshId::_COUNT) ? k_RockMesh : 0;
			const auto rock = explode_object::PackMesh(rockMesh);
			for (int i = 0; i < k_Rocks; ++i)
			{
				const float dz = -k_RockSpread + effect.Random(2.0f * k_RockSpread);
				const float dx = -k_RockSpread + effect.Random(2.0f * k_RockSpread);
				const glm::vec3 position(data.centre.x + dx, data.centre.y, data.centre.z + dz);
				const float scale =
				    k_RockScale * k_RockScaleMin + effect.Random(k_RockScale * k_RockScaleMax - k_RockScale * k_RockScaleMin);
				const float angle = effect.Random(6.28318548f); // 2 pi
				// rows X = (cos, 0, sin) s, Y = (0, s, 0), Z = (-sin, 0, cos) s, the matrix rows being the axes here
				const glm::mat3 axes(affine::PlacementMatrix(glm::vec3(0.0f), angle, scale));
				explode_object::QueueMesh(rock, axes, position, explosion::BlastOrigin(data.centre), blastSpeed,
				                          k_ExplodeSpread);
			}
		}
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"),
			                   "Explosion: started at ({:.1f}, {:.1f}, {:.1f}), margin {:.1f}, search r {:.1f} ({} cells), {} "
			                   "targets, {} rocks queued",
			                   data.centre.x, data.centre.y, data.centre.z, margin, r, cells, data.targets.size(),
			                   explode_object::QueuedCount());
			for (const auto& target : data.targets)
			{
				const auto p = PositionOf(target.object);
				SPDLOG_LOGGER_INFO(
				    spdlog::get("game"), "Explosion:   target {} at ({:.1f}, {:.1f}) d {:.1f} radius {:.1f}{}{}{}",
				    static_cast<uint32_t>(target.object), p.x, p.z,
				    gutils::GetDistanceInMetres(glm::vec2(data.centre.x, data.centre.z), glm::vec2(p.x, p.z)),
				    ecs::object::Get2DRadius(target.object),
				    Locator::entitiesRegistry::value().AllOf<ecs::components::Tree>(target.object) ? " tree" : "",
				    Locator::entitiesRegistry::value().AllOf<ecs::components::Villager>(target.object) ? " villager" : "",
				    Locator::entitiesRegistry::value().AnyOf<ecs::components::Abode, ecs::components::StoragePit>(target.object)
				        ? " abode"
				        : "");
			}
		}
	}

	/// UR_Explosion's update, called at the end of every collection step
	void Update(Effect& effect, CollectionData& data) const
	{
		TestShots();
		data.spread += effect.GetDt() * spreadSpeed;
		if (!Locator::entitiesRegistry::has_value())
		{
			return;
		}
		auto& registry = Locator::entitiesRegistry::value();
		if (data.beamFx != entt::null && !registry.Valid(data.beamFx))
		{
			data.beamFx = entt::null;
		}
		const unsigned int turn = game_clock::Turn();
		data.anyTarget = false;
		for (auto& target : data.targets)
		{
			if (target.object != entt::null)
			{
				if (IsAvailable(target.object))
				{
					target.turn = turn;
				}
				else
				{
					target.object = entt::null;
				}
			}
			if (target.object != entt::null)
			{
				data.anyTarget = true;
			}
		}
		if (data.exploded >= maxObjectsToExplode && data.deleted >= maxObjectsToDelete)
		{
			data.anyTarget = false;
		}
		if (!data.anyTarget)
		{
			data.targets.clear();
		}
		const auto refresh = [&](CollectionData::Target& target) {
			if (target.object != entt::null)
			{
				if (IsAvailable(target.object))
				{
					target.turn = turn;
				}
				else
				{
					target.object = entt::null;
				}
			}
		};
		// one object per step: the first that answers the CanBeDestroyed query stops the walk
		bool stop = false;
		for (size_t i = 0; i < data.targets.size() && !stop; ++i)
		{
			auto& target = data.targets[i];
			// only objects with a 3D object (a mesh)
			if (target.object == entt::null || !registry.AllOf<ecs::components::Mesh>(target.object))
			{
				continue;
			}
			glm::vec3 p = PositionOf(target.object);
			const glm::vec3 d = p - data.centre;
			// reached by the ring: |p - centre|^2 <= (GetRadius + spread)^2, in 3D: (dz dz + dy dy) + dx dx against
			// (R + spread) x (R + spread)
			const float reach = ecs::object::GetRadius(target.object) + data.spread;
			if (reach * reach < (d.z * d.z + d.y * d.y) + d.x * d.x)
			{
				continue;
			}
			if (const auto* sphere = shields::FindShieldContainingPoint(p, k_TargetShieldMargin); sphere != nullptr)
			{
				shields::AddImpactTarget(*sphere, p);
				const SpellEventInfo event {.type = SpellEventInfo::Type::HitSpell,
				                            .position = p,
				                            .velocity = p - data.centre,
				                            .strength = 1.0f,
				                            .checkShields = false,
				                            .target = shields::SpellOf(*sphere)};
				if (effect.SendSpellEvent(event) == 0)
				{
					target.object = entt::null;
				}
			}
			refresh(target);
			// The object to affect is the object itself (the redirection of citadel parts to the citadel is not
			// ported)
			if (target.object == entt::null)
			{
				continue;
			}
			p = PositionOf(target.object);
			const SpellEventInfo query {.type = SpellEventInfo::Type::CanDestroy,
			                            .position = p,
			                            .velocity = glm::vec3(0.0f),
			                            .strength = 1.0f,
			                            .checkShields = false,
			                            .target = target.object};
			if (effect.SendSpellEvent(query) == 1)
			{
				stop = true;
				refresh(target);
				if (target.object != entt::null && !ecs::fire::traits::IsCreature(target.object) &&
				    data.exploded < maxObjectsToExplode)
				{
					++data.exploded;
					// The object's world matrix and 3D mesh go to the explode queue (from centre - 5 m at BlastSpeed),
					// thrown to pieces by the EXPLODE_OBJECT effect (Rules/ExplodeObject.cpp)
					explode_object::QueueObject(target.object, explosion::BlastOrigin(data.centre), blastSpeed,
					                            k_ExplodeSpread);
				}
				if (target.object != entt::null && data.deleted < maxObjectsToDelete)
				{
					++data.deleted;
					if (Trace())
					{
						SPDLOG_LOGGER_INFO(
						    spdlog::get("game"),
						    "Explosion: object {} at ({:.1f}, {:.1f}) destroyed by the beam (spread {:.1f}, {} exploded, {} "
						    "deleted)",
						    static_cast<uint32_t>(target.object), p.x, p.z, data.spread, data.exploded, data.deleted);
					}
					if (k_DestroyByBeam)
					{
						explosion::DestroyedByBeam(target.object);
					}
				}
			}
			target.object = entt::null;
		}
	}
};

/// SetPSysCloseDown (only the base properties): the effect closes, for every atom that passes the condition
class SetPSysCloseDown final: public Modifier
{
public:
	bool ModifyAtom(Effect& effect, Atom& /*atom*/, Collection::Slot& /*slot*/) const override
	{
		effect.CloseDown();
		return true;
	}
};

/// UR_ChangeScaleXYZ: StartTime, StopTime, StartScaleXZ, StopScaleXZ, StartScaleY, StopScaleY
class ChangeScaleXYZ final: public Modifier
{
public:
	explicit ChangeScaleXYZ(const Object& object)
	    : startTime(object.Float("StartTime", 0.0f))
	    , stopTime(object.Float("StopTime", 0.0f))
	    , startXZ(object.Float("StartScaleXZ", 0.0f))
	    , stopXZ(object.Float("StopScaleXZ", 0.0f))
	    , startY(object.Float("StartScaleY", 0.0f))
	    , stopY(object.Float("StopScaleY", 0.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		explosion::ChangeScaleXYZ(effect.AtomAge(atom), effect.GetDt(), startTime, stopTime, startXZ, stopXZ, startY, stopY,
		                          atom.ruleScale, atom.stretch);
		return true;
	}
	float startTime, stopTime, startXZ, stopXZ, startY, stopY;
};

/// UR_MoveAtom: StartTime, StopTime, MoveSmoothly, StartX/Y/Z, StopX/Y/Z; moves the atom's position in its
/// collection's frame
class MoveAtom final: public Modifier
{
public:
	explicit MoveAtom(const Object& object)
	    : startTime(object.Float("StartTime", 0.0f))
	    , stopTime(object.Float("StopTime", 0.0f))
	    , smoothly(object.Bool("MoveSmoothly", false))
	    , start(object.Float("StartX", 0.0f), object.Float("StartY", 0.0f), object.Float("StartZ", 0.0f))
	    , stop(object.Float("StopX", 0.0f), object.Float("StopY", 0.0f), object.Float("StopZ", 0.0f))
	{
	}
	bool ModifyAtom(Effect& effect, Atom& atom, Collection::Slot& /*slot*/) const override
	{
		explosion::MoveAtom(effect.AtomAge(atom), effect.GetDt(), startTime, stopTime, smoothly, start, stop, atom.position);
		return true;
	}
	float startTime, stopTime;
	bool smoothly;
	glm::vec3 start, stop;
};
} // namespace

bool explosion::ChangeScaleXYZ(float age, float dt, float startTime, float stopTime, float startXZ, float stopXZ, float startY,
                               float stopY, float& ruleScale, float& stretch)
{
	if (age < startTime)
	{
		return false;
	}
	float xz = stopXZ;
	float y = stopY;
	if (age <= stopTime)
	{
		// (port guard) StopTime == StartTime would divide by 0: taken as the end
		const float t = stopTime > startTime ? (age - startTime) / (stopTime - startTime) : 1.0f;
		xz = startXZ + (stopXZ - startXZ) * t;
		y = startY + (stopY - startY) * t;
	}
	else if (age - dt > stopTime)
	{
		return false; // only the first step after StopTime writes the stop values
	}
	ruleScale = xz;
	// a flat XZ has no stretch
	stretch = xz > 0.0001f ? y / xz : 0.0f;
	return true;
}

bool explosion::MoveAtom(float age, float dt, float startTime, float stopTime, bool smoothly, const glm::vec3& start,
                         const glm::vec3& stop, glm::vec3& out)
{
	if (age < startTime || age > stopTime)
	{
		return false;
	}
	float t = stopTime > startTime ? (age - startTime) / (stopTime - startTime) : 1.0f; // (port guard) as above
	if (dt + age >= stopTime)
	{
		t = 1.0f; // the last step lands on the stop point
	}
	if (smoothly)
	{
		t = t * t * (3.0f - 2.0f * t); // smoothstep
	}
	out = start + (stop - start) * t;
	return true;
}

bool explosion::CanBeDestroyedBySpell(entt::entity object, entt::entity /*spell*/)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(object))
	{
		return false;
	}
	using namespace ecs::components;
	// the classes that say no: creatures, fields and the citadel (openblack's Temple and its parts, the worship sites
	// and their totems)
	if (registry.AnyOf<Creature, Field, Temple, TempleInteriorPart, CitadelWorship, WorshipSite, WorshipTotem>(object))
	{
		return false;
	}
	// Any other object: whether it receives effects. (inferred) An object flag and the script test (an object in a
	// running script, then a flag of the spell) are not tracked by openblack: taken as clear / not in a script
	const ecs::effects::EffectValues none;
	return ecs::effects::IsEffectReceiver(object, none);
}

void explosion::DestroyedByBeam(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(object))
	{
		return;
	}
	using namespace ecs::components;
	if (registry.AnyOf<Abode, StoragePit, SpellDispenser, TotemStatue, Field>(object))
	{
		// A building loses all its life (built: life 0); below 1 every inhabitant reacts as to a tap on the abode, the
		// building stops being functional / the town's emergency, and with a town and no site a building site is added
		// (site life = 1.1 x life - 0.1). With the site and no destruction mesh the drawn percentage is min(the built
		// percentage, 0.98 x life) = 0: the building is gone from view until it is repaired. Done through
		// ecs::abodes::ReduceLife (a field changes nothing). (approximate) Totem statues and a dispenser without an
		// Abode use the generic object life reduction
		ecs::abodes::ReduceLife(object, ecs::life::LifeOf(object), std::nullopt);
		return;
	}
	// Any other object is deleted
	if (registry.AnyOf<Tree, DeadTree>(object))
	{
		ecs::DeleteTree(object);
		return;
	}
	if (registry.AllOf<Animal>(object))
	{
		ecs::animal_ai::Remove(object);
		return;
	}
	if (registry.AllOf<Villager>(object))
	{
		// Its dependants dropped and the deletion, no death (ECS/Villager/VillagerDeath.h)
		ecs::villager::Delete(object);
		return;
	}
	// the rest (rocks, mobile objects and statics, features, piles): deleted, as when destroyed by an effect
	ecs::fire::traits::DestroyedByEffect(object);
}

void openblack::psys::RegisterExplosionRules()
{
	RegisterModifier("UR_Explosion", MakeModifierOf<Explosion>);
	RegisterModifier("SetPSysCloseDown", MakeModifierOf<SetPSysCloseDown>);
	RegisterModifier("UR_ChangeScaleXYZ", MakeModifierOf<ChangeScaleXYZ>);
	RegisterModifier("UR_MoveAtom", MakeModifierOf<MoveAtom>);
	explode_object::RegisterRules(); // UR_ExplodeObject, UR_ExplodeObject2 (SF_ExplodeObject)
}
