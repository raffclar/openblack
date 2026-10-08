/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/fwd.hpp>

#include "ScriptHeaders/ScriptEnums.h"

/// What a thing is to the scripts: its SCRIPT_OBJECT_TYPE and its sub-type (the index of its info record). The finders
/// CALL 026, CALL_NEAR 051 and the ones still to port (CALL_IN 055, CALL_IN_NEAR 067, CALL_NEAR_IN_STATE 316, the
/// script containers' SET_SCRIPT_STATE) filter with them. Only the reverse direction (CHL CREATE, CHLApi
/// CreateScriptObject) existed before. The per-class values were read from every class of the original.
///
/// openblack has no classes: the components stand in for them. A class openblack does not have (Reward, Dance, Ball,
/// LandscapeVortex, ScriptTimer, Scaffold, the computer player, the ScriptMarker, which keeps only a Transform) gives
/// ObjectType::None, as the original's base class does for the classes without an override.
namespace openblack::ecs::script_type
{

/// The CHL "any sub-type"
constexpr uint32_t k_AnySubtype = 5000;
/// The "no sub-type": the filter never accepts it, not even for an equal sub-type
constexpr uint32_t k_NoSubtype = 9999;

/// The script object type. Per class: Abode = 2 (every abode class: Field, StoragePit, TownCentre, Totem, Creche,
/// Graveyard, Windmill, Workshop, Wonder, Football, PuzzleTotem), Feature = 3 (also AnimatedStatic, the chess pieces,
/// Flowers, WorshipSiteUpgrade), Villager = child ? 5 : 4, Animal = 6, the flying species and the Vulture = 21,
/// Reward = 7, MobileStatic = 8 (Bonfire, Fragment, MagicTeleport and the street lanterns too), Rock = 33, Town = 9,
/// Dance = 10, Flock = 11, Creature = 12, DeadTree = 13 (FelledTree too), InfluenceRing = 14, WeatherThing = 15,
/// Pot = 16 (every pile), ScriptTimer = 17, CitadelHeart = 18, WorshipSite = 19, MobileObject = 20 (Whale,
/// OneOffSpellSeed, crops, creeds...), Tree = 22 (MagicTree too), LandscapeVortex = 23, SpellSeed = from a one-shot
/// spell ? 30 : 24, Poo = 25, Ball = 28, Mist = 29, SpellDispenser = 36, ScriptHighlight = 37, the computer player = 38,
/// Scaffold = 39, TotemStatue = 40, ScriptMarker = 1; every other class 0 (the Citadel, PuzzleGame, BigForest,
/// FishFarm, MapShield, the spell icons...)
[[nodiscard]] script::ObjectType TypeOf(entt::entity thing);

/// By TypeOf (types 2..40), the index of the thing's info record in its class's info array; k_NoSubtype, with
/// "Unknown type for search", for a type without one (TOWN, DANCE, FLOCK, INFLUENCE_RING, WEATHER_THING, TIMER,
/// CITADEL, BALL, MIST, ONE_SHOT_SPELL_IN_HAND, TOTEM, COMPUTER_PLAYER, SCAFFOLD, below 2 or above 40), and with
/// "Not implemented" for DEAD_TREE
[[nodiscard]] uint32_t SubtypeOf(entt::entity thing);

/// The CALL / CALL_NEAR filter (thing, type, subtype): TypeOf == type, and subtype k_AnySubtype or
/// SubtypeOf == subtype (never when SubtypeOf is k_NoSubtype; SubtypeOf is not asked for k_AnySubtype)
[[nodiscard]] bool Matches(entt::entity thing, script::ObjectType type, uint32_t subtype);

} // namespace openblack::ecs::script_type
