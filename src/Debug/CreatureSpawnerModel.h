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

#include <bitset>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include "Creature/CreatureDesires.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/CreaturePlanner.h"
#include "Creature/LeashRules.h"
#include "Enums.h"

// What the Creature Spawner debug window works out before it acts: the names it shows, the size a creature is drawn
// at, the player's creature as the window's top section reads it, the desires strongest first, where things are put
// round a creature, and the window's own random picks. Pure functions and small value types, tested with hand-made
// values.

namespace openblack::debug::creature_spawner
{

/// The window's own random picks (facing, where a mark goes) come from a private engine started from this seed, so
/// they never touch the game's random streams
inline constexpr uint32_t k_RandomSeed = 3333u;
using RandomEngine = std::mt19937;

/// The species a creature can be, from the cow on, as the game numbers them
inline constexpr size_t k_SpeciesCount = static_cast<size_t>(CreatureType::_COUNT) - 1;

/// How a species reads in the window; "Unknown" outside the species list
[[nodiscard]] std::string_view SpeciesName(CreatureType species);
/// The species by its place in the list, from the cow
[[nodiscard]] CreatureType SpeciesAt(size_t index);

/// How a player reads in the window
[[nodiscard]] std::string_view OwnerName(PlayerNames owner);

/// The scale a creature is drawn at: the drawn pose's when it has one (smaller in its temple's pen), else its own
[[nodiscard]] glm::vec3 DrawnScale(const std::optional<glm::vec3>& poseScale, const glm::vec3& ownScale);

/// A stage of growing up as the window may set it, kept within the stages there are
[[nodiscard]] int32_t ClampPhase(int phase, int32_t lastPhase);

/// The leashes a creature knows, by their names in the game's order, or "none"
[[nodiscard]] std::string KnownLeashes(const std::bitset<creature_leash::k_Types.size()>& known);

/// The desires worth listing, strongest first: those switched on that have a source or any strength. Equal ones keep
/// the game's order.
[[nodiscard]] std::vector<creature_desires::Desire> DesiresByStrength(const creature_desires::Desires& desires);

/// The player's creature as the window's top section shows it, from its components; each part is none when the
/// creature lacks that component
struct PlayerCreatureSummary
{
	entt::entity entity {entt::null};
	CreatureType species {CreatureType::Unknown};
	glm::vec3 position {0.0f};
	/// Its own size, and the scale it is drawn at
	float size {1.0f};
	glm::vec3 drawnScale {1.0f};
	float alignment {0.0f};
	std::optional<uint32_t> developmentPhase;
	std::optional<glm::vec3> home;
	/// The leash it wears, if any; the leashes it knows; whether it is the one its owner can lead
	std::optional<LeashType> wornLeash;
	std::string knownLeashes;
	bool leashable {false};
	/// Its strongest desires, and the plan it carries out
	std::vector<creature_desires::Desire> desires;
	std::optional<creature_planner::Plan> plan;
	std::optional<creature_physiology::Needs> needs;
};

/// The parts of a creature the summary reads; a missing component is a null pointer
struct PlayerCreatureParts
{
	entt::entity entity {entt::null};
	CreatureType species {CreatureType::Unknown};
	float size {1.0f};
	float alignment {0.0f};
	bool leashable {false};
	glm::vec3 position {0.0f};
	glm::vec3 ownScale {1.0f};
	std::optional<glm::vec3> poseScale;
	std::optional<uint32_t> developmentPhase;
	std::optional<glm::vec3> home;
	std::optional<LeashType> wornLeash;
	std::bitset<creature_leash::k_Types.size()> knownLeashes;
	const creature_desires::Desires* desires {nullptr};
	std::optional<creature_planner::Plan> plan;
	const creature_physiology::Needs* needs {nullptr};
};

/// The summary of a creature, with at most `maxDesires` desires
[[nodiscard]] PlayerCreatureSummary Summarise(const PlayerCreatureParts& parts, size_t maxDesires);

/// A point `distance` across the land from a creature at `position` facing by `rotation`, turned `angle` radians from
/// straight ahead (positive to its right), on the creature's height
[[nodiscard]] glm::vec3 PointAround(const glm::vec3& position, const glm::mat3& rotation, float angle, float distance);

/// A point on the ground under another, as a creature teleported there stands
[[nodiscard]] glm::vec3 OnGround(const glm::vec3& point, float groundHeight);

/// A facing in degrees picked at random, 0 to 360
[[nodiscard]] float RandomFacingDegrees(RandomEngine& random);
/// A texel of the skin picked at random, away from its edges
[[nodiscard]] uint8_t RandomSkinTexel(RandomEngine& random);
/// One of a list's items picked at random; the list must not be empty
[[nodiscard]] size_t RandomIndex(RandomEngine& random, size_t count);

/// A name from a mind file as plain text: anything outside ASCII reads as '?'
[[nodiscard]] std::string Narrow(std::u16string_view text);

/// Whether a file of the game's mind folder is a creature's mind: the bodies the game saves beside the minds are not
[[nodiscard]] bool IsMindFileName(std::string_view fileName);

} // namespace openblack::debug::creature_spawner
