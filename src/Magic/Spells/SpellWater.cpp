/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellWater.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fields.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/ScreenshotRequestSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Trees.h"
#include "ECS/WaterRings.h"
#include "Game.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "SpellClasses.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

namespace
{
water::SpellWaterData& MutableDataOf(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* data = registry.TryGet<water::SpellWaterData>(spell); data != nullptr)
	{
		return *data;
	}
	return registry.Assign<water::SpellWaterData>(spell);
}

float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// The object's 2D radius: 5 m for a field, else the mesh's half extent x the scale (ecs::object::GetRadius). No
/// kind of the water's targets has its own radius.
float ObjectRadius(entt::entity object)
{
	return ecs::object::GetRadius(object);
}

/// The ring of a drop: the first free of the 1024 ring slots, age 0, the growth, the angle, aspect 1.0, rate 1.0,
/// cell 0x30 and the colour. The colour is one of the constants k_RippleColours, not the landscape light table: it is
/// written as is (no seaLight) and kept for the ring's life (ecs::AddWaterRing). Nothing when the 1024 slots are full.
bool AddDropRing(const glm::vec3& position, float growth, float angle, uint32_t argb)
{
	const ecs::WaterRing ring {.position = position,
	                           .age = 0,
	                           .growth = growth,
	                           .angle = angle,
	                           .aspect = 1.0f,
	                           .rate = 1.0f,
	                           .cell = 0x30,
	                           .argb = argb};
	return ecs::AddWaterRing(ring);
}

/// OPENBLACK_TEST_WATER_SHOT="<turns>,<path>[;<turns>,<path>...]" (test hook, not in the original): a screenshot that
/// many game turns after a water spell's first drop (docs/bw1-notes/openblack-internals.md)
struct Shot
{
	unsigned int turns;
	std::string path;
	bool done {false};
};

/// What the water hook keeps between calls, in the debug hooks' store (Locator::debugHooks)
struct SpellWaterDebugHooksState
{
	std::optional<std::vector<Shot>> shots; // parsed on first use
	// A map here, not a component on the spell: test-only state that keeps its entries after the spell is gone
	std::unordered_map<entt::entity, unsigned int> firstTurn; // each spell's first drop turn, never cleared
};

SpellWaterDebugHooksState& SpellWaterDebugHooksData()
{
	if (!Locator::debugHooks::has_value())
	{
		std::fputs("magic::spell_water: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return Locator::debugHooks::value().Get<SpellWaterDebugHooksState>();
}

std::vector<Shot>& Shots()
{
	auto& shots = SpellWaterDebugHooksData().shots;
	if (shots.has_value())
	{
		return *shots;
	}
	shots = [] {
		std::vector<Shot> list;
		const char* value = std::getenv("OPENBLACK_TEST_WATER_SHOT");
		if (value == nullptr)
		{
			return list;
		}
		std::stringstream stream(value);
		std::string item;
		while (std::getline(stream, item, ';'))
		{
			const auto comma = item.find(',');
			if (comma != std::string::npos)
			{
				list.push_back({static_cast<unsigned int>(std::atoi(item.substr(0, comma).c_str())), item.substr(comma + 1)});
			}
		}
		return list;
	}();
	return *shots;
}

void ShotHook(entt::entity spell)
{
	auto& firstTurn = SpellWaterDebugHooksData().firstTurn;
	if (Shots().empty() || !Locator::screenshotRequest::has_value())
	{
		return;
	}
	const unsigned int turn = CurrentTurn();
	const auto first = firstTurn.try_emplace(spell, turn).first->second;
	for (auto& shot : Shots())
	{
		if (!shot.done && turn >= first + shot.turns)
		{
			shot.done = true;
			Locator::screenshotRequest::value().Request(shot.path);
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Water test: screenshot {} turns after the first drop (turn {}) -> {}",
			                   shot.turns, turn, shot.path);
			return; // one request per frame
		}
	}
}

/// The water spell's turn: one drop per game turn
int Process(entt::entity entity)
{
	// the base spell's turn first (maintain, the PSys step); its result is returned on every path
	const int result = base::Process(entity);
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return result;
	}
	auto& data = MutableDataOf(entity);
	// a finished putting-out-fire reaction is forgotten
	if (data.puttingOutFireReaction != 0 &&
	    !ecs::effects::reactions::IsAvailable(ecs::effects::reactions::Find(data.puttingOutFireReaction)))
	{
		data.puttingOutFireReaction = 0;
	}
	ShotHook(entity); // test hook (OPENBLACK_TEST_WATER_SHOT), also while the spell closes
	const auto& spell = registry.Get<const Spell>(entity);
	if (spell.closedDown)
	{
		return result;
	}
	// C = (castPos.x, the land's height at castPos + castPos.y, castPos.z): only x and z are used below
	const glm::vec3 centre(spell.castPos.x, LandAt(spell.castPos.x, spell.castPos.z) + spell.castPos.y, spell.castPos.z);
	// r = GameFloatRand(R) x 0.7 + 0.3, then a = GameFloatRand(2 pi)
	const float radius = water::RainRadius(spell.magicType);
	const float r = water::DropDistance(game_random::GameFloatRand(radius));
	const float a = game_random::GameFloatRand(glm::two_pi<float>());
	glm::vec3 drop(centre.x + r * std::cos(a), 0.0f, centre.z + r * std::sin(a));
	// P.y = the land's height at P + 0.2
	drop.y = LandAt(drop.x, drop.z) + 0.2f;
	// a point spell event at P (no movement, strength 1, no target) with the default spell effect: burn
	// -4000 x strength x tribal power within 1 m cools fires, costPerEvent 10, reaction 21.
	// Its result is ignored: the drop reaches the objects and makes its ring even without chants.
	const psys::SpellEventInfo event {.type = psys::SpellEventInfo::Type::Point,
	                                  .position = drop,
	                                  .velocity = glm::vec3(0.0f),
	                                  .strength = 1.0f,
	                                  .checkShields = false,
	                                  .target = entt::null};
	OpsOf(spell.spellClass).spellEvent(entity, event);
	if (!registry.Valid(entity))
	{
		return result;
	}
	// the 3 x 3 cells from P's (a spiral, 9 steps): every object whose edge is within 2.5 x the spell's power of P gets
	// ApplyWaterSpell. The distance is 2D (the table hypotenuse of the map coordinates' x, z).
	// There is no "done" set and no own-cell test: a multi-cell object (a field) in several of the 9 cells gets
	// ApplyWaterSpell once per cell, as in the original
	// the spiral walks the drop's map coordinates, adding each step to the whole cells only (the fraction stays and the
	// 16-bit add wraps at the map's edge)
	const auto dropCoords = map_coords::FromMetres(glm::vec2(drop.x, drop.z));
	auto coords = dropCoords;
	map_coords::Spiral spiral;      // direction 1 and count 1
	std::string watered;            // the trace's list
	for (int i = 0; i < 9; ++i)
	{
		const auto cell = map_coords::Cell(coords);
		// the fixed list, then the mobile one, from the heads
		for (const auto object : ecs::map_cells::ObjectsInCell(glm::ivec2(cell)))
		{
			if (!registry.Valid(object) || object == entity)
			{
				continue;
			}
			const auto* transform = registry.TryGet<const Transform>(object);
			if (transform == nullptr)
			{
				continue;
			}
			// the table hypotenuse on the 16.16 map coordinates (Common/GUtilsDistance)
			const float distance = gutils::GetDistanceInMetres(
			    dropCoords, map_coords::FromMetres(glm::vec2(transform->position.x, transform->position.z)));
			if (water::InReach(distance, ObjectRadius(object)))
			{
				water::ApplyWaterSpell(object, entity);
				if (TraceEnabled())
				{
					watered += fmt::format(" {}", static_cast<uint32_t>(object));
				}
			}
		}
		map_coords::AddCells(coords, spiral.Next());
	}
	// a ring when k_RippleEvery < age - lastRipple
	const auto& after = registry.Get<const Spell>(entity);
	if (water::RippleDue(after.age, data.lastRipple))
	{
		data.lastRipple = after.age;
		const auto colour = water::k_RippleColours[game_random::GameRand(5)];
		const float angle = game_random::GameFloatRand(glm::two_pi<float>());
		AddDropRing(drop, water::RippleGrowth(after.magicType), angle, colour);
	}
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(
		    spdlog::get("game"),
		    "Spell trace: spell {} SpellWater drop at ({:.2f}, {:.2f}, {:.2f}) r {:.2f} of R {:.0f}: watered [{} ], "
		    "age {:.2f}, last ring {:.2f}, chants {:.1f}",
		    static_cast<uint32_t>(entity), drop.x, drop.y, drop.z, r, radius, watered, after.age, data.lastRipple,
		    after.chants);
	}
	return result;
}

int InitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	// both fields cleared at allocation
	MutableDataOf(spell) = water::SpellWaterData {};
	return base::InitWithPos(spell, position, castData, info);
}

/// Any object: a burning object makes the spell start REACT_TO_MAGIC_WATER_PUTTING_OUT_FIRE (34) with the spell's
/// player, once (while puttingOutFireReaction holds it). Returns 0.
float ObjectApplyWaterSpell(entt::entity object, entt::entity entity)
{
	auto& data = MutableDataOf(entity);
	if (ecs::fire::IsOnFire(object) && data.puttingOutFireReaction == 0)
	{
		const auto& spell = Locator::entitiesRegistry::value().Get<const Spell>(entity);
		// (inferred) a spell without a player passes the neutral player, as the other CreateReaction callers
		data.puttingOutFireReaction = ecs::effects::reactions::CreateReaction(
		    entity, openblack::Reaction::ReactToMagicWaterPuttingOutFire, spell.player, true);
		if (TraceEnabled())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: spell {} water on burning entity {} -> reaction {} (34)",
			                   static_cast<uint32_t>(entity), static_cast<uint32_t>(object), data.puttingOutFireReaction);
		}
	}
	return 0.0f;
}
} // namespace

float water::RainRadius(MagicType type)
{
	// 22 -> 6.0, 23 -> 12.0, else 1.0
	switch (type)
	{
	case MagicType::Water:
		return 6.0f;
	case MagicType::WaterPowerUpOne:
		return 12.0f;
	default:
		return 1.0f;
	}
}

float water::RippleGrowth(MagicType type)
{
	// 22 -> 2.0, 23 -> 4.0, else 1.0
	switch (type)
	{
	case MagicType::Water:
		return 2.0f;
	case MagicType::WaterPowerUpOne:
		return 4.0f;
	default:
		return 1.0f;
	}
}

bool water::RippleDue(float age, float lastRipple)
{
	// the difference is rounded to single precision before the strict 0.1 < difference test
	const volatile float difference = age - lastRipple;
	return k_RippleEvery < difference;
}

float water::ApplyWaterSpell(entt::entity object, entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(object) || !registry.Valid(entity))
	{
		return 0.0f;
	}
	if (registry.AllOf<Tree>(object))
	{
		// a tree: the any-object part first (its result dropped)
		ObjectApplyWaterSpell(object, entity);
		const auto& spell = registry.Get<const Spell>(entity);
		// WATER_PU1 grows full grown trees past their size and never seeds a sapling
		const auto sapling = ecs::ApplyWaterSpell(object, spell.magicType == MagicType::WaterPowerUpOne);
		if (sapling != entt::null && spell.hasPlayer)
		{
			// a statistic kept only in a multiplayer game, nothing here. The alignment for a new tree: good.
			ecs::effects::alignment::UpdateForTree(spell.player, true);
			if (TraceEnabled())
			{
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: spell {} water on tree {}: sapling {} (good alignment)",
				                   static_cast<uint32_t>(entity), static_cast<uint32_t>(object),
				                   static_cast<uint32_t>(sapling));
			}
		}
		return 1.0f;
	}
	if (registry.AllOf<Field>(object))
	{
		// a field: the any-object part, then (with a player) the player's creature may learn to copy watering crops
		// (with the plain water miracle, whichever water it was), then, if it is not on fire, sow or grow
		const float result = ObjectApplyWaterSpell(object, entity);
		if (const auto& spell = registry.Get<const Spell>(entity); spell.hasPlayer)
		{
			ecs::creature_mimic::Consider(spell.player, creature_watching::Deed::CastWaterOnCrops, object, MagicType::Water);
		}
		if (!ecs::fire::IsOnFire(object))
		{
			const auto* field = registry.TryGet<const Field>(object);
			const auto crops = field->crops;
			const float growth = field->growth;
			ecs::ApplyWaterSpellToField(object);
			if (TraceEnabled())
			{
				SPDLOG_LOGGER_INFO(
				    spdlog::get("game"),
				    "Spell trace: spell {} water on field {}: crops {} -> {}, growth {:.1f} -> {:.1f}, food {:.2f}",
				    static_cast<uint32_t>(entity), static_cast<uint32_t>(object), crops, field->crops, growth, field->growth,
				    field->food);
			}
		}
		return result;
	}
	return ObjectApplyWaterSpell(object, entity);
}

const water::SpellWaterData* water::DataOf(entt::entity spell)
{
	return Locator::entitiesRegistry::value().TryGet<const SpellWaterData>(spell);
}

void openblack::magic::RegisterWaterSpell()
{
	// the water spell overrides only the turn; the rest is the base spell's
	const SpellOps ops {.initWithPos = InitWithPos,
	                    .initWithObject = base::InitWithObject,
	                    .process = Process,
	                    .spellEvent = spell_event::SpellEvent,
	                    .costToMaintain = base::CalculateCostToMaintain,
	                    .closeDown = base::CloseDown,
	                    .toBeDeleted = nullptr,
	                    .hasEnoughChantsForRecast = base::HasEnoughChantsAndLifeForRecast,
	                    .particleType = base::GetParticleType};
	RegisterOps(SpellClass::Water, ops);
}
