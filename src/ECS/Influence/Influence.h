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

#include <array>
#include <optional>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/LandMorph.h"
#include "Enums.h"

namespace openblack
{
struct GInfluenceInfo;
}

// A player's influence: where the player can act and cast. Sources: the citadel and the player's towns (a radius
// each: inside it the influence is 1) and the influence rings (a gradient). Wiki: docs/bw1-notes/magic.md.
//   Influence.cpp            the queries and the land's globals
//   InfluenceSources.cpp     town and citadel radii
//   InfluenceRings.cpp       influence rings
//   InfluenceCircles.cpp     the border circles drawn in the world and the hand's crossing of them
//   InfluenceDebugHooks.cpp  OPENBLACK_TEST_INFLUENCE

namespace openblack::influence
{
/// How the influence is calculated: only the virtual influence reads it (not ported). The hand's in-influence test
/// passes Interface.
enum class CalcType : int32_t
{
	Default = 0,
	Interface = 1,
};

/// The player's influence at a point, -1..1; > 0 = in the player's influence. The cast rules and the hand test `> 0`
/// with allies on.
[[nodiscard]] float CalculatePlayerInfluence(PlayerNames player, const glm::vec3& position, CalcType type = CalcType::Default,
                                             bool includeAllies = true);
[[nodiscard]] float CalculatePlayerInfluence(entt::entity player, const glm::vec3& position, CalcType type = CalcType::Default,
                                             bool includeAllies = true);
/// Citadel + towns + rings, clamped to -1..1 (an anti ring of the player covering the point gives 0)
[[nodiscard]] float CalculatePlayerRawInfluence(PlayerNames player, const glm::vec3& position);
[[nodiscard]] bool IsInPlayerRawInfluence(PlayerNames player, const glm::vec3& position);
/// Inside an anti-influence ring of that player
[[nodiscard]] bool IsInAntiInfluence(PlayerNames player, const glm::vec3& position);
[[nodiscard]] bool IsInAntiInfluence(entt::entity player, const glm::vec3& position);
/// The ring gradient: 1 up to 0.4 r, 0.8 -> 0 up to 0.6 r, 0.2 -> 0 up to r
[[nodiscard]] float CalculateInfluenceOnRange(float distance, float radius);
[[nodiscard]] float CalculateInfluenceOnRange(float distance, float radius, const GInfluenceInfo& info);

// ---- rings (InfluenceRings.cpp) ----

/// A fixed ring (CREATE_INFLUENCE_RING, INFLUENCE_POSITION, shields)
entt::entity CreateRing(const glm::vec3& position, PlayerNames player, float radius, bool anti);
/// A ring that follows an object (INFLUENCE_OBJECT); entt::null if the object is not valid
entt::entity CreateRingOnObject(entt::entity object, PlayerNames player, float radius, bool anti);
void DeleteRing(entt::entity ring);
/// Attached rings follow their object, or go with it
void ProcessRings();

// ---- towns and citadel (InfluenceSources.cpp) ----

/// The influence part of the town update for every town (the town's base influence + that of its abodes, x
/// townInfluenceMultiplier)
void ProcessTowns();
/// A town entity's influence radius (0 if it has no influence yet)
[[nodiscard]] float TownRadius(entt::entity town);
/// The influence radius of a temple entity
[[nodiscard]] float CitadelRadius(entt::entity temple);

// ---- the player's influence power (InfluenceSources.cpp) ----

/// The player's influence power: the radius of the player's citadel (with its heart), + each town's radius, + the
/// radius of every influence ring of the player (anti rings too), each a float sum; stored and returned. The rest of
/// the original's calculation (a running average and the stats history) has no reader in openblack and is not ported.
float CalculateInfluencePower(PlayerNames player);
/// CalculateInfluencePower for every player and the neutral one, as the player update does after its alignment:
/// MagicLoop's slot 3
void CalculateInfluencePowers();
/// The power as the last CalculateInfluencePower left it (0 before the first one of the land)
[[nodiscard]] float InfluencePower(PlayerNames player);
/// The sum (from 0, in float) of the power of every active player and then the neutral one, divided by the player's
/// own power; 0 when the own is 0. (inferred) a player is active when the land made it (magic::players::EntityOf):
/// openblack keeps that only for the human one
[[nodiscard]] float InfluencePowerRatio(PlayerNames player);

/// The influence part of the citadel update for every temple (one per player): NoteInfluence on the radius against
/// the last one, and the border latch the citadel object sets (ShowBoundary)
void ProcessCitadels();

/// What the original does each turn at the influence slot: ProcessRings, plus the towns' influence (run in the towns'
/// own loop in the original) and the citadels'
void ProcessTurn();

// ---- the influence circles and the hand that crosses them (InfluenceCircles.cpp) ----

/// The colour of a player's circle and ripple by colour index, ARGB with alpha 0 (the draw writes the alpha). 5 and 7
/// differ from the generic player colours (0x4777FF and black there). The colour index is the player's remapped
/// index: (inferred) the identity, as every openblack user of the player colours (SurfRevol.cpp, TownBelief.cpp)
/// takes it
inline constexpr std::array<uint32_t, 8> k_CircleColours = {
    0x00FF4646u, // red
    0x0047FF54u, // green
    0x00E347FFu, // magenta
    0x0047F9FFu, // cyan
    0x00FFFD47u, // yellow
    0x004664FFu, // blue
    0x00FFA247u, // orange
    0x00FFFFFFu, // white
};

/// One influence circle: the curtain of land_morph::InfluenceCurtain and its overlap state
struct Circle
{
	PlayerNames player; ///< The colour index & 7
	glm::vec3 centre;   ///< The citadel's / town's position (y only counts in the containment test)
	float radius;
	land_morph::Curtain curtain;       ///< 3N + 3 vertices, 4N triangles
	std::vector<uint8_t> hidden;       ///< One per column (N + 1); 1 = inside another circle of the player
	bool dead {false};                 ///< Inside another circle of the player, deleted when added
	uint32_t alphaCache {0xFFFFFFFFu}; ///< The alpha the draw last gave the middle row

	/// N, the segments
	[[nodiscard]] size_t Segments() const { return curtain.positions.size() / 3 - 1; }
};

/// The circles are rebuilt at the next Update3DInfluence when |now - last| > 0.01 (strict). The citadel and town
/// updates call it with their radius against the last one
void NoteInfluence(float now, float last);
/// Sets the dirty flag. The original calls it when a citadel or a town is deleted, a town changes owner, and from the
/// town centre update. openblack deletes no town or temple and no town changes hands yet: no caller
void ForceNeedUpdateInfluence();
/// From the turn update: only when the dirty flag is set and GameTurn % 10 == 0. The circles are reset, then every
/// player up to the neutral one gets a circle for the citadel when its radius is not 0 and one per town of its list
/// whose radius is not 0; then the dirty flag is cleared
void Update3DInfluence();
/// The circles, newest first. Empty without a land
[[nodiscard]] std::span<const Circle> Circles();

/// The player's border is shown. Cleared for all eight players on every land load (here the land registry's reset);
/// the citadel's 3D object sets it when its fade reaches 1. Until then the player's circles are drawn with alpha 0 and
/// crossing them makes no ripple and no sound
[[nodiscard]] bool BoundaryShown(PlayerNames player);
/// Marks the player's border as shown
void ShowBoundary(PlayerNames player);

/// The world view: nothing at all (no clock step, no material) when the camera's y <= 100; else the middle row's
/// alpha: 120 from y = 200 up, below that (y - 100) x 0.01 x 120, truncated
[[nodiscard]] std::optional<uint8_t> CurtainAlpha(float cameraY);
/// Every circle before it is drawn: when `alpha` differs from its cache and its player's border is shown, the cache =
/// alpha and the middle vertex (3j + 1) of every column j <= N not hidden gets (rgb) | alpha << 24; the ground and top
/// rows keep alpha 0
void SetCurtainAlpha(uint8_t alpha);

/// The ripple a hand crossing a border makes: 7 sprites of smoke.raw cell 63 in the plane of the border, growing and
/// fading for 2 s
struct Ripple
{
	static constexpr size_t k_Sprites = 7;
	/// (approximate) the Z-sorter's point: the original never initialises it, so its key reads whatever the heap held
	/// there; the crossing point here
	glm::vec3 point {0.0f};
	int32_t life {0};    ///< ms (2000 at creation). (inferred) an int: the game time step is taken off it
	uint32_t colour {0}; ///< k_CircleColours[player]; the draw writes its alpha byte for each sprite
	/// Rows (c, 0, s), (s, 0, -c), (0, 1, 0) and the point (glm columns), c and s of the yaw: its XZ plane stands upright
	/// along the border
	glm::mat4 matrix {1.0f};
	std::array<float, k_Sprites> sizes {};
	std::array<float, k_Sprites> angles {};
	std::array<int32_t, k_Sprites> flags {}; ///< 1 at creation, 0 on a wrap; nothing traced reads it
};
/// One sprite of a ripple as the draw hands it to the sprite renderer
struct RippleSprite
{
	float size;
	float angle;
	uint32_t argb; ///< The ripple's colour with this sprite's alpha
};
/// For every ripple, once a frame: life -= the game time step; below 0 it is removed and its sprites released. The Z
/// object each one left then queues is the renderer's (Renderer::CollectInfluenceRipples)
void UpdateRipples(uint32_t gameTimeIncMs);
/// The ripples, newest first
[[nodiscard]] std::span<const Ripple> Ripples();
/// The Z object's callback, for ripple `index` of Ripples(): every sprite grows by the game time step x 0.001 x 10,
/// wraps past 14 and takes the alpha (1 - s / 14) x fade x 255 (truncated), fade = life x 0.001 under 1000 ms, else 1.
/// The sizes are kept: the next draw grows them again
[[nodiscard]] std::array<RippleSprite, Ripple::k_Sprites> DrawRipple(size_t index, uint32_t gameTimeIncMs);

/// Once a frame while the game is not paused: the hand's point is compared against the circles Update3DInfluence
/// keeps (one per citadel and per town with influence) and, for each player whose "the hand is inside" changed since
/// the last frame and who has a circle whose edge the hand crossed since the previous call, makes a ripple at the
/// crossing; then G_HandThroughInfluence_01 (InGame 52) plays once, 3D at the hand
void ProcessHandCrossing(const glm::vec3& handPosition);

/// ProcessHandCrossing without the sound: true when a crossing made a ripple
[[nodiscard]] bool HandCrossedInfluence(const glm::vec3& handPosition);

namespace detail
{
/// For the tests only: the hand crossing's statics back to the process start; nothing in the original clears them
void ResetHandCrossing();
/// The point of the circle's edge between `inside` and `outside` (both at y = 0), by halves: while dx^2 + dz^2 > 1
/// the midpoint (a + b) x 0.5 replaces the end on its side (the circle's containment test); then the midpoint of the
/// last two
[[nodiscard]] glm::vec3 CrossingPoint(const glm::vec3& centre, float radius, glm::vec3 inside, glm::vec3 outside);
} // namespace detail

// ---- the land's globals ----

/// SET_LAND_NUMBER (0 = no story land): the map script's globals (Locator::mapScriptSystem)
[[nodiscard]] int32_t LandNumber();
/// SET_TOWN_INFLUENCE_MULTIPLIER / SET_PLAYER_INFLUENCE_MULTIPLIER (1 before each map script):
/// the map script's globals (Locator::mapScriptSystem)
[[nodiscard]] float TownInfluenceMultiplier();
[[nodiscard]] float PlayerInfluenceMultiplier();
/// Set at start from the registry value "GatheringFlag" in the original: every player has influence 1 everywhere.
/// Only a test hook sets it.
void SetInfluenceEverywhere(bool on);
[[nodiscard]] bool IsInfluenceEverywhere();

/// OPENBLACK_TEST_INFLUENCE / OPENBLACK_INFLUENCE_EVERYWHERE (InfluenceDebugHooks.cpp); call once per turn
void RunDebugHooks();
} // namespace openblack::influence
