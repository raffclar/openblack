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

#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string_view>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::creature
{
struct CreatureMind;
}

namespace openblack::ecs
{
class Registry;
}
namespace openblack::ecs::systems
{
class LeashSystemInterface;
}
namespace openblack::state_hash
{
class Hasher;
}
namespace openblack::ecs::components
{
struct Transform;
}

/// The player's own creature: the one the profile brings to a land (LOAD_MY_CREATURE) and the script natives that
/// act on it. Its data lives on the creature's components; nothing here keeps state
namespace openblack::ecs::player_creature
{

/// The profile's mind file in the given folder; none when the profile names no file
[[nodiscard]] std::optional<std::filesystem::path> ProfileMindPath(std::string_view file,
                                                                   const std::filesystem::path& mindFolder);

/// CURRENT_PROFILE_HAS_CREATURE: whether the profile's mind file exists in Scripts/CreatureMind. The profile's file is
/// the creature-file setting (EngineConfig::profileCreatureFile)
[[nodiscard]] bool ProfileHasCreature();

/// What LOAD_MY_CREATURE makes of the profile's mind file: the species and body the file describes, and the middle of
/// the map cell of the point it is given, on the ground (y is the height above it, 0). What the file leaves out
/// stays as the species starts it
struct LoadPlan
{
	CreatureType species;
	std::optional<float> size;
	std::optional<float> alignment;
	float strength;
	glm::vec3 position;
};

/// The plan, or none: when the player has a creature already, or the file is missing, unreadable or of no species
[[nodiscard]] std::optional<LoadPlan> PlanLoad(bool playerHasCreature, const creature::CreatureMind* mind, glm::vec2 pointXZ);

/// LOAD_MY_CREATURE: the local player's creature, read from the profile's mind file and made at the point's map cell;
/// nothing when PlanLoad gives none
entt::entity LoadMyCreature(glm::vec2 pointXZ);

/// The "creature" part of the state hash: each creature's owner, species, body, growing-up stage and home
void RegisterStateHash();
/// What that part hashes, in the creatures' storage order; nothing without a creature
void HashCreatures(state_hash::Hasher& h, const Registry& registry);

/// The highest stage of growing up, a fully mature creature
inline constexpr int32_t k_LastDevelopmentStage = 13;

/// CALL_PLAYER_CREATURE: the creature the player leads, if they have one
[[nodiscard]] std::optional<entt::entity> PlayersCreature(const systems::LeashSystemInterface& leash, PlayerNames player);

/// SET_CREATURE_HOME's point as the original keeps a home: x and z in its fixed point, the height dropped (the home is
/// on the ground)
[[nodiscard]] glm::vec3 HomeOnGround(glm::vec3 point, float groundHeight);

/// SET_CREATURE_HOME: a creature's home, where it is kept while it starts to grow up; anything else is left alone
void SetHome(systems::LeashSystemInterface& leash, const Registry& registry, entt::entity thing, glm::vec3 home);

/// Which of a temple mesh's special points is the place it keeps its player's creature
inline constexpr size_t k_TemplePenPoint = 15;

/// Where a temple keeps its player's creature: its mesh's pen point turned and moved with the temple, or none when the
/// mesh marks no such point
[[nodiscard]] std::optional<glm::vec3> TemplePenPoint(const components::Transform& temple,
                                                      std::span<const glm::mat4> specialPoints);

/// Each turn, while its player's temple stands built, a creature's home is that temple's pen point (x and z in the
/// fixed point, on the ground), whatever a script set before. Without a creature nothing is looked up
void FollowTemplePens(systems::LeashSystemInterface& leash, const Registry& registry,
                      const std::function<std::optional<glm::vec3>(PlayerNames)>& penOf,
                      const std::function<float(glm::vec2)>& groundAt);

/// FollowTemplePens with the game's temples, meshes and land
void FollowTemplePens();

/// A creature in its temple's pen is drawn smaller, down to a newborn's size, the nearer it is to the pen's place; its
/// own size is kept
inline constexpr float k_PenDrawnSize = 0.22f;
/// The ramp: the own size at this distance from the pen's place and farther, the pen's size at the inner one and nearer
inline constexpr float k_PenOuterRadius = 16.0f;
inline constexpr float k_PenInnerRadius = 14.0f;
/// The pen's walls, from the temple's turn: the first wall's angle after the temple's, and the angle between the walls
inline constexpr float k_PenWallAngle = 3.83f;
inline constexpr float k_PenWallsApart = 0.897598f;

/// Whether a point (x, z) is between the pen's two walls of a temple at `temple` turned by `templeYAngle`
[[nodiscard]] bool BetweenPenWalls(glm::vec2 temple, float templeYAngle, glm::vec2 point);

/// The size a creature is drawn at: its own, or within the pen's outer radius and between its walls, the ramp down
/// to the pen's size
[[nodiscard]] float PenDrawnSize(float size, float distanceToPen, bool betweenWalls);

/// Each turn, the size every creature is drawn at, from its distance to its home and its player's built temple's pen
/// walls (components::CreatureDrawPose::scale); none outside a pen
void ShrinkInPens();

/// SET_CREATURE_DEV_STAGE: a creature's stage of growing up, 0 to 13; its desires follow in the mind's next turn.
/// Anything else, or a stage out of range, is left alone
void SetDevelopmentStage(Registry& registry, entt::entity thing, int32_t stage);

/// DEV_FUNCTION, the parts that act on the local player's creature: 2 lets it learn the rope leash, 3 the good and
/// evil leashes and makes it the one its player leads. False for a function that is not ported (nothing is done)
bool DevFunction(systems::LeashSystemInterface& leash, int32_t function, PlayerNames player);

/// CREATURE_IN_DEV_SCRIPT: whether a creature is in its growing-up scripts; anything else is left alone
void SetInDevScript(Registry& registry, entt::entity thing, bool inDevScript);

} // namespace openblack::ecs::player_creature
