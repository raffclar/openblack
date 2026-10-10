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

#include <optional>
#include <span>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>

#include "Enums.h"

/// How the scripts read and write a thing's properties, where the answer is a rule of its own
namespace openblack::script::property_rules
{

/// A building of a town, as the scripts weigh whether the town is wholly destroyed: its life (0 to 1), whether it is a
/// field, and how much of it is built (0 to 1)
struct TownBuilding
{
	float life {1.0f};
	bool field {false};
	float built {1.0f};
};

/// A building still stands while more than this much of it is built (or it is finished)
constexpr float k_StandingBuilt = 0.1f;

/// A town is wholly destroyed when none of its buildings stands: one stands while it has life, isn't a field and is
/// more than a tenth built. A town without buildings is destroyed.
[[nodiscard]] bool TownCompletelyDestroyed(std::span<const TownBuilding> buildings);

/// A thing's player as the scripts number them: 1 for a wholly destroyed town, whoever it belonged to; nothing for a
/// thing of no player or of the neutral player; otherwise the player's number counted from 1
[[nodiscard]] float PlayerProperty(std::optional<PlayerNames> player, bool destroyedTown);

/// Whether a thing is in a creature's hand, as the scripts ask: it is what the creature carries, or what it is eating
[[nodiscard]] bool InCreatureHand(entt::entity thing, entt::entity carried, std::optional<entt::entity> eating);

/// A creature's need that the scripts can set
enum class CreatureNeed : uint8_t
{
	Warmth,
	Energy,
	Itchiness,
	Poo,
	Exhaustion,
	Dehydration,
};

/// The value a creature's need takes when a script sets it: warmth is kept between -1 and 1, energy between 0 and 1,
/// the others are taken as they are
[[nodiscard]] float SetNeed(CreatureNeed need, float value);

/// The scripts give and take angles in degrees; the game turns them to and from radians with these factors
constexpr float k_RadiansToTurns = 0.159154937f;
constexpr float k_DegreesInATurn = 360.0f;
constexpr float k_DegreesToRadians = 0.0174532924f;
[[nodiscard]] float AngleToScript(float radians);
[[nodiscard]] float AngleFromScript(float degrees);

/// Whether a script may set a thing's life: a thing the scripts hold while they own the widescreen bars, and a thing
/// made indestructible, can't be brought to a hundredth of its life or under it
constexpr float k_LowestProtectedLife = 0.01f;
[[nodiscard]] bool CanSetLife(float life, bool heldDuringCutscene, bool indestructible);

/// A creature of size 1 stands this tall to the scripts
constexpr float k_CreatureHeightPerSize = 15.0f;
/// The size a creature takes for a height a script gives, as the game divides by its height per size
constexpr float k_CreatureSizePerHeight = 0.0666666701f;
[[nodiscard]] float CreatureHeight(float size);
[[nodiscard]] float CreatureSizeForHeight(float height);

/// A placed thing's angles in radians: its lean about the across axis, its turn about the upright axis and its lean
/// about the forward axis. The world's things are placed turned the other way by them.
struct Angles
{
	float x {0.0f};
	float y {0.0f};
	float z {0.0f};
};
/// The angles of a thing placed the way it faces. A thing that doesn't lean keeps its whole turn, past a quarter either
/// way.
[[nodiscard]] Angles PlacedAngles(const glm::mat3& rotation);
/// The way a thing placed at these angles faces
[[nodiscard]] glm::mat3 PlacedRotation(const Angles& angles);

/// A thing's belief in a player, as the scripts ask it: a town's belief in the player (none it was given is no belief);
/// anything else believes wholly in its own player and not at all in the others
[[nodiscard]] float BeliefForPlayer(bool town, std::optional<float> townBelief, std::optional<PlayerNames> owner,
                                    PlayerNames player);

} // namespace openblack::script::property_rules
