/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <span>

#include <glm/vec3.hpp>

#include "Creature/CreatureAudio.h"
#include "Creature/CreatureFootprints.h"
#include "Creature/CreatureLayers.h"
#include "ECS/Components/CreatureBody.h"

/// The footsteps of a creature's body, and the steps in the water that scare the fish of a fish puzzle. The frames
/// sound the footsteps of the drawn body. The game turns find the footsteps of the body of the turn, so that every
/// computer scares the fish in the same turn.
namespace openblack::ecs::creature_turn_steps
{

/// A foot lower than this height scares the fish near it
constexpr float k_ScareFootHeight = 1.0f;

/// The layers of a body posed with these animations played by weight, and with this face
[[nodiscard]] creature_audio::Layers LayersOf(const components::CreatureAnimation& animation,
                                              std::span<const components::CreatureAnimation::Slot> slots,
                                              const creature_layers::FaceLayer& face);

/// The layers of the body as the game turn posed it: the animations of the turn and the face of the world
[[nodiscard]] creature_audio::Layers TurnLayersOf(const components::CreatureAnimation& animation);

/// The point where a footstep among these events scares the fish. The step is under the lower foot, the left foot only
/// when it is lower than the right foot. Nothing when no event is a footstep, or when that foot is not lower than
/// k_ScareFootHeight.
[[nodiscard]] std::optional<glm::vec3> ScarePoint(std::span<const creature_audio::FiredEvent> fired,
                                                  const creature_footprints::Feet& feet);

/// The point where the body's footsteps of one game turn scare the fish, from the layers of the last turn to the
/// layers of this turn, with the feet where this turn posed them. Nothing for a body with no feet. The layers of this
/// turn are kept in last for the next turn, also when the body has no feet.
[[nodiscard]] std::optional<glm::vec3> StepScare(creature_audio::Layers& last, const creature_audio::Layers& current,
                                                 float turnMs, const creature_audio::InfoOf& infoOf,
                                                 const std::optional<creature_footprints::Feet>& feet);

} // namespace openblack::ecs::creature_turn_steps
