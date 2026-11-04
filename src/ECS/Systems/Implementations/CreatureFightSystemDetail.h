/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/LandAvoid.h"
#include "Common/GameHeading.h"
#include "Creature/CreatureArena.h"
#include "Creature/CreatureEyes.h"
#include "Creature/FightTranscript.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureFight.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Enums.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs
{
class Registry;
}
namespace openblack::graphics
{
class L3DMesh;
}

/// What the files of the creature fight system share: the length of a game turn, the helpers that more than one file
/// uses and the arena helpers that the fight stages use. Not for use elsewhere.
namespace openblack::ecs::systems::fight_detail
{
using components::CreatureAnimation;
using components::CreatureFighting;
using components::CreatureKnockedOut;
using components::CreatureLocomotion;

/// The milliseconds of a game turn for the creature: a frozen creature has fewer, down to none. Its fight animations,
/// its cast hold, the tests of its blows and the drips of its blood move on by these.
int32_t TurnMsOf(const CreatureAnimation* animation);

float HeightOf(float size);

glm::vec2 Flat(const glm::vec3& point);

float GroundAt(glm::vec2 point);

std::string_view Name(CreatureFighting::Stage stage);

std::string_view Name(CreatureKnockedOut::Stage stage);

/// The levels of a creature for the fight transcript: its fight health and stamina while it fights, its life, its
/// strength, its exhaustion and its energy
fight_transcript::Fields Levels(const ecs::Registry& registry, entt::entity creature);

/// A change of one level of a creature for the fight transcript, with what caused it
fight_transcript::Fields LevelChange(entt::entity creature, std::string_view level, float before, float after,
                                     std::string_view why);

float DurationOf(entt::entity creature, size_t animation);

bool Has(entt::entity creature, size_t animation);

/// A direction in the world as seen by a creature facing a heading: x to its right, y ahead
glm::vec2 WorldToLocal(glm::vec2 world, GameHeading heading);

/// Moves a creature where it stands, both where it is drawn from and to this turn
void Shift(CreatureLocomotion& locomotion, glm::vec2 by);

/// The creature's mesh and the matrix it is drawn with, if it has a loaded mesh
std::optional<std::pair<const graphics::L3DMesh*, glm::mat4>> DrawnMeshOf(const ecs::Registry& registry, entt::entity creature);

/// Whether the hand of the player holds nothing: no thing and no miracle. A hand that only starts to take a thing holds
/// nothing yet.
/// The player whose hand this computer moves. The fight view, the sparkles, the presses, the help and the music
/// follow the creature of this player. The turns use the sender of each command.
PlayerNames OwnPlayer();

/// Whether a human god plays the player: the player has hands and no computer mind plays it. A computer god has hands
/// too. Each computer has the same hands and minds, thus the turns can read this.
bool IsHumanGod(PlayerNames player);

/// Whether the creature still fights its opponent. The opponent is in the duel, or the opponent left the duel before the
/// creature and the fight of the opponent has not ended.
bool FightsOn(const ecs::Registry& registry, entt::entity creature, const CreatureFighting& fighting);

/// Whether the opponent of the creature has its order to start its duel against the creature, but its body did not let
/// it start yet. The creature that started the fight starts its duel at least one turn after the other creature.
bool OpponentStartsDuel(const ecs::Registry& registry, entt::entity creature, const CreatureFighting& fighting);

namespace arena = openblack::creature_arena;

/// Where creatures may go on the land, worked out the first time it is asked for on a land
const land_avoid::Map& LandAvoidOf(ecs::Registry& registry);

/// The creature that took an arena leaves its fight: an arena made for the fight goes, any other is free again
void ReleaseArena(ecs::Registry& registry, entt::entity entity);

/// The creature that started a fight takes its arena after the face-off: the arena appears for both fighters. Without
/// an arena near enough, or room for one, it takes none.
bool DrawArenaFor(ecs::Registry& registry, entt::entity creature, entt::entity opponent);

/// How far the posed body of a creature reaches in front of its feet, in the direction that it faces. A fighter uses its
/// bones as they were at the end of its last game turn, measured from where it stands now.
float ForwardExtentOf(const ecs::Registry& registry, entt::entity creature);

/// When either fighter is not inside the range of the arena, the fighter makes no moves. In its stance, it steps back to
/// the middle when it is farther from the middle than its body radius and its species has that step. A block stops.
/// Returns whether either fighter is not inside the range.
bool KeepInArena(ecs::Registry& registry, entt::entity creature, CreatureFighting& fighting);
} // namespace openblack::ecs::systems::fight_detail
