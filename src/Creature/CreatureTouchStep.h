/*******************************************************************************
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

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Common/InputSteps.h"
#include "Creature/CreatureFeedback.h"
#include "ECS/Components/HandOnCreature.h"

/// One step of the hand that holds the creature: the step weighs a slap or a stroke, as the game did once a frame at the
/// screen's refresh. Pure: the creature's answers come from the callbacks, so that tests can use fakes.
namespace openblack::creature_hand
{

/// What the step knows about the hand
struct TouchStepInput
{
	/// The time of the step, in milliseconds
	float stepMs {input_steps::k_StepMs};
	/// The places at the hand's distance on the lines through the cursor now and at the last step
	glm::vec3 here {0.0f};
	glm::vec3 before {0.0f};
	/// Whether the line of sight touches the body
	bool onBody {false};
	/// How high the hand showed at the last step above the land under it, which gives the height of a slap
	float handHeight {0.0f};
	glm::vec2 cursor {0.0f};
	float screenWidth {0.0f};
	/// The creature's height as it shows
	float creatureHeight {0.0f};
};

/// The answers of the creature to the step
struct TouchAnswers
{
	/// The creature reels from a slap: whether it takes the reaction now
	std::function<bool(const creature_feedback::Slap&)> slap;
	/// The creature gets the reward of a stroke on a part: whether it takes the reaction now
	std::function<bool(creature_feedback::BodyPart)> stroke;
	/// The part nearest the touch, none when the parts are not known
	std::function<std::optional<creature_feedback::BodyPart>()> nearestPart;
	/// A third hard slap in a row at one height bruises the creature at this column of its skin
	std::function<void(uint8_t)> bruise;
};

/// One step of stroking and slapping. It tells whether the hand slapped.
bool WeighTouchStep(ecs::components::HandOnCreature& contact, const TouchStepInput& input, const TouchAnswers& creature);

/// The cursor of a step of a frame, at its place between the cursor of the last frame and the cursor of this frame, to
/// the nearest pixel
[[nodiscard]] glm::ivec2 StepCursor(glm::ivec2 before, glm::ivec2 now, const input_steps::Steps& steps, uint32_t step);

} // namespace openblack::creature_hand
