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

#include <functional>
#include <optional>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureIdleMind.h"

/// The agendas of the actions scripts force on creatures to stage their scenes (the creatures of the glade looking,
/// pointing and sitting), and of a script turning a creature to face a point or sending it somewhere.
///
/// Facing the camera only means anything to a creature with a player: one without (as the glade's creatures) doesn't
/// turn to the camera or look at it, but still points at it.
namespace openblack::creature_mind
{
/// Looking at a thing until something else is given: it turns to face it, holding still two seconds, then watches it
/// for a day
constexpr float k_FaceThingSeconds = 2.0f;
constexpr float k_LookForeverSeconds = 86400.0f;
/// Looking at a thing without going up to it: watching its foot for 1.1 seconds and up to 1.2 more
constexpr float k_LookButDontApproachSeconds = 1.1f;
constexpr float k_LookButDontApproachExtraSeconds = 1.2f;
/// Looking at the camera lasts five seconds
constexpr float k_LookAtCameraSeconds = 5.0f;
/// Pointing at a thing lasts five seconds, and at the camera one; pointing never lasts less than a second
constexpr float k_PointAtThingSeconds = 5.0f;
constexpr float k_PointAtCameraSeconds = 1.0f;
constexpr float k_ShortestPointSeconds = 1.0f;
/// Sitting where a script says, it turns to face the place, holding still a tenth of a second
constexpr float k_FacePlaceSeconds = 0.1f;

/// Turning to face a thing and watching it for a day
[[nodiscard]] std::vector<Step> LookForever(uint32_t thing);
/// Turning to face a thing and watching its foot a little while; chance is from 0 to 1 and picks how long
[[nodiscard]] std::vector<Step> LookButDontApproach(uint32_t thing, float chance);
/// Watching the camera for five seconds, curious, for a creature with a player; done at once for one without
[[nodiscard]] std::vector<Step> LookAtCamera(bool hasPlayer);
/// Pointing at a thing for five seconds wherever it goes, amazed
[[nodiscard]] std::vector<Step> PointAtThing(uint32_t thing);
/// Pointing at the camera for a second, amazed, having turned to face it first when it has a player
[[nodiscard]] std::vector<Step> PointAtCamera(bool hasPlayer, glm::vec3 camera);
/// Sitting down where a script says: it walks there until within its own radius, turns to face the place, turns to
/// face down the slope, then sits for ten seconds and up to five more
[[nodiscard]] std::vector<Step> SitDownAt(uint32_t place, glm::vec2 point, float radius, const Random& random);
/// Turning to face a point, watching it, as a script asks
[[nodiscard]] std::vector<Step> FaceForScript(glm::vec3 point);

/// A creature a script sends to a point walks there in two goes, watching the point. With no distance given, it first
/// gets within a sixth of the way (at least its height, and no more than 0.7 of its height and 22), then within its
/// height; with one, it gets within that distance twice.
[[nodiscard]] std::vector<Step> MoveForScript(glm::vec2 from, glm::vec3 point, float height, float distance);
constexpr float k_FirstLegShare = 1.0f / 6.0f;
constexpr float k_FirstLegHeightShare = 0.7f;
constexpr float k_FirstLegMost = 22.0f;

/// Facing a point as a script asks: whether it has to turn, given how far the point is and how far round from the way
/// it faces (radians, either way)
[[nodiscard]] bool NeedsToTurnToFace(float distance, float turn);
/// It is right on top of a point nearer than this, and faces it within this much of a turn
constexpr float k_OnTopOf = 0.1f;
constexpr float k_FacingWithin = 0.392699093f;

/// The ground around a creature it looks down the slope at: ten units away in the eight directions about it
constexpr float k_SlopeLookDistance = 10.0f;
/// The lowest of the ground about a point, the first of the lowest when several are as low. heightAt says how high the
/// ground is at a point, none off the land; with all of them off the land the game takes the corner of the map at the
/// origin.
[[nodiscard]] glm::vec2 DownSlope(glm::vec2 from, const std::function<std::optional<float>(glm::vec2)>& heightAt);
} // namespace openblack::creature_mind
