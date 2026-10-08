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

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/Zoomer.h"
#include "Enums.h"

// The flock miracles: SpellFlock (a spell with objects) and its two classes, SpellFlockFlying (doves, or bats for an
// evil player) and SpellFlockGround (wolves). The animals are real ones of the animals' AI (ECS/AnimalAI.h: CreateAnimal,
// MoveTo, SetStateRaw...); what the spell doves, bats and wolves do differently is kept here: the 20-turn fade instead
// of dying, and the wolves' corridor.
// Research: docs/bw1-notes/miracles.md ("Flocks").

namespace openblack::magic
{

/// The flock spell's own data
struct SpellFlockData
{
	entt::entity flock {entt::null}; ///< The Flock (components::Flock), new at InitWithPos
	int created {0};                 ///< The animals tried so far (a skipped one counts too)
	float emitted {0.0f};            ///< The emit accumulator (12 per second)
	/// The last spawn point: MapCoords x, z (6553.6 per metre, truncated) and the height above the land (metres)
	glm::ivec2 lastSpawn {0};
	float lastSpawnHeight {0.0f};
	uint32_t castEffect {0};   ///< (flying only) The cast effect, a PSys without a spell stepped every frame
	unsigned int castTurn {0}; ///< (openblack) the game turn of InitWithPos, for OPENBLACK_TEST_FLOCK_SHOT
};

/// What the spell doves, bats and wolves add to their Animal: the fade (a Zoomer on the alpha 0..255) and the wolf's
/// corridor and player
struct SpellFlockAnimal
{
	entt::entity spell {entt::null};
	bool wolf {false};
	Zoomer fade;                   ///< Value 255, destination 255 at creation
	glm::vec2 normal {1.0f, 0.0f}; ///< The unit normal of the start -> destination line (x, z)
	float offset {0.0f};           ///< -(normal . start)
	float halfWidth {0.0f};        ///< The hunting radius (GMagicFlockGroundInfo.huntingRadius, 45 m)
	glm::vec2 destination {0.0f};  ///< The wolf's final destination (also given to the animals' AI)
	glm::vec3 previous {0.0f};     ///< Where it was at the last spell turn (the shield test's segment)
};

namespace spell_flock
{
constexpr float k_EmitPerSecond = 12.0f; ///< Animals emitted per second
constexpr float k_AngleVariation = 2.0f; ///< The fan-out angle spread over the whole flock
constexpr float k_SpawnJitter = 0.1f;    ///< GameFloatRand(0.2) - 0.1 m on x and z
constexpr float k_FlyingScale = 2.8f;    ///< GetScale() x 2.8 + GameFloatRand(3 - 2.8)
constexpr float k_FlyingScaleTop = 3.0f;
constexpr float k_GroundScale = 1.5f; ///< GetScale() x 1.5 + GameFloatRand(2 - 1.5)
constexpr float k_GroundScaleTop = 2.0f;
constexpr float k_MinTravel = 10.0f;  ///< The travel distance is halved while it is above this
constexpr float k_WolfArrive = 30.0f; ///< A wolf dies within 30 m of the end of its run
constexpr int k_TurnsToDieOver = 20;  ///< The fade's length in turns
constexpr float k_FullAlpha = 255.0f;
constexpr int k_MagicObjectCreated = 9; ///< SPOT_VISUAL_TYPE of the wolves' puff

/// numberToCreate x the tribal power, rounded to nearest
[[nodiscard]] int NumberToCreate(uint32_t numberToCreate, float tribalPower);
/// The rain effect is FLOCK_FLYING_RAIN_EVIL (125) when the player's alignment < alignmentSwitch, else GOOD (124); the
/// cast effect of InitWithPos is 122 / 123 the same way
[[nodiscard]] bool IsEvil(float alignment, float alignmentSwitch);
/// The flying animal: GAnimalInfo 21 SpellBat (evil) or 20 SpellDove
[[nodiscard]] AnimalInfo FlyingAnimal(float alignment, float alignmentSwitch);

/// The flight direction d (x, z): a human caster's camera forward, otherwise castPos - handPos; (1, 0) when
/// |d|^2 < 0.0001
[[nodiscard]] glm::vec2 Direction(bool human, glm::vec3 cameraForward, glm::vec3 castPos, glm::vec3 handPos);
/// The side an animal fans out to: a human caster's cross product of v = normalize(spawn - castPos) with the normalised
/// d (v.z d.x - v.x d.z): +1 above 0.1, -1 below -0.1; otherwise (and for the AI) +1 for an even `created`, -1 for an
/// odd one
[[nodiscard]] float Side(bool human, glm::vec2 direction, glm::vec2 spawn, glm::vec2 castPos, int created);
/// The fan-out angle: k_AngleVariation x created x side / numberToCreate
[[nodiscard]] float Angle(int created, float side, int numberToCreate);
/// A rotation about Y (row vector): (x cos - z sin, x sin + z cos)
[[nodiscard]] glm::vec2 Rotate(glm::vec2 d, float angle);
/// The destination for a distance: d set to that length, then the spawn point's MapCoords with their 10 m cell (the
/// high words) moved to trunc((cell x 10 + d) / 10), the sub-cell part kept. `spawn` in MapCoords.
[[nodiscard]] glm::ivec2 DestinationAt(glm::ivec2 spawn, glm::vec2 direction, float distance);
/// The destination T of a new animal; from 800 m (distanceToTravel) halved while T is off the map and the next
/// distance is over 10 m. `inBounds` is MapCoords::InBounds. False when T is still off the map.
template <class InBounds>
bool Destination(glm::ivec2 spawn, glm::vec2 direction, float distance, InBounds inBounds, glm::ivec2& out)
{
	do
	{
		out = DestinationAt(spawn, direction, distance);
		distance *= 0.5f;
	} while (!inBounds(out) && distance > k_MinTravel);
	return inBounds(out);
}

/// The spawn loop's point between the last two hand positions: f = (created - prevEmit) / (emit - prevEmit); x and z
/// are old + round((new - old) x f), the height old + (new - old) x f
[[nodiscard]] glm::ivec2 SpawnPoint(glm::ivec2 from, glm::ivec2 to, float f);

/// MapCoords (x, z) <-> metres: trunc(x x 6553.6) and x x 10 / 65536
[[nodiscard]] glm::ivec2 ToMapCoords(glm::vec2 metres);
[[nodiscard]] glm::vec2 ToMetres(glm::ivec2 mapCoords);

/// The wolf's corridor: the normal (DZ, -DX) / |D| of D = destination - start ((1, 0) when |D|^2 < 0.0001) and its
/// offset -(normal . start); both in metres
void SetupCorridor(SpellFlockAnimal& wolf, glm::vec2 start, glm::vec2 destination, float halfWidth);
/// The end of a wolf's run: GetDistanceInMetres(position, destination) < 30 -> SetDying (the state itself is the
/// animals': ECS/AnimalPredators.cpp SpellWolfMoveToPos)
[[nodiscard]] bool WolfArrived(const SpellFlockAnimal& wolf, glm::vec2 position);
/// |normal . p + offset| <= halfWidth, and the point is not more than halfWidth behind the wolf along the corridor
/// (measured from the corner of the wolf's 10 m cell, its MapCoords' high words)
[[nodiscard]] bool IsPosOnCorridor(const SpellFlockAnimal& wolf, glm::vec2 wolfPosition, glm::vec2 point);

/// The flock animals' SetDying: while the fade's destination is not 0 (once only), the Zoomer goes to 0
/// (SetDestinationWithSpeedAndTime(0, 0, t)) over the turns to die over (20 x 100 ms = 2 s). The animal never dies the
/// normal way: it keeps moving while it fades.
void StartFade(SpellFlockAnimal& animal);
/// One turn of the fade, from the spell's object loop; true when the alpha is exactly 0 (ToBeDeleted)
[[nodiscard]] bool ProcessFade(SpellFlockAnimal& animal);

// ---- for the animals' AI (a spell wolf hunts inside its corridor) ----

/// The spell data of an animal of a flock miracle, nullptr for any other
[[nodiscard]] const SpellFlockAnimal* AnimalOf(entt::entity animal);
/// IsPosOnCorridor on the entities (false when `wolf` is not a flock wolf); the wolves' hunting target check and prey
/// search (ECS/AnimalPredators.cpp) use it
[[nodiscard]] bool IsOnCorridor(entt::entity wolf, glm::vec2 point);
} // namespace spell_flock
} // namespace openblack::magic
