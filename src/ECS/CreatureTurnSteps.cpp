/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTurnSteps.h"

#include <algorithm>
#include <vector>

#include "Creature/CreaturePointing.h"

namespace openblack::ecs::creature_turn_steps
{

using components::CreatureAnimation;

creature_audio::Layers LayersOf(const CreatureAnimation& animation, std::span<const CreatureAnimation::Slot> slots,
                                const creature_layers::FaceLayer& face)
{
	creature_audio::Layers layers;
	// The animations played by weight stand in for the body's own action
	if (slots.empty() && creature_layers::IsPlaying(animation.body))
	{
		layers.body = creature_audio::Played {creature_layers::CurrentAnimation(animation.body), animation.body.timeMs};
	}
	if (animation.gesture.animation.has_value())
	{
		layers.gesture = creature_audio::Played {*animation.gesture.animation, animation.gesture.timeMs};
	}
	// A face being run back to its start to change makes no sound
	if (face.current.has_value() && face.wanted == face.current)
	{
		layers.face = creature_audio::Played {*face.current, face.timeMs};
	}
	std::vector<float> weights;
	std::vector<size_t> animations;
	for (const auto& slot : slots)
	{
		layers.slots.push_back({slot.animation, slot.timeMs});
		weights.push_back(slot.weight);
		animations.push_back(slot.animation);
	}
	// The game moves the four animations of a point on as the first of them, so only its events sound, whatever the
	// weights
	const auto pointing =
	    creature_audio::BlendSoundingSlot(animations, creature_pointing::k_Animations, creature_pointing::k_Animations.front());
	layers.soundingSlot = pointing.has_value() ? pointing : creature_audio::SoundingSlot(weights);
	return layers;
}

creature_audio::Layers TurnLayersOf(const CreatureAnimation& animation)
{
	return LayersOf(animation, animation.slots, animation.face);
}

std::optional<glm::vec3> ScarePoint(std::span<const creature_audio::FiredEvent> fired, const creature_footprints::Feet& feet)
{
	if (std::ranges::none_of(fired, [](const auto& event) { return creature_audio::IsFootstep(event.event.action); }))
	{
		return std::nullopt;
	}
	const auto right = creature_footprints::FootOf(feet.right);
	const auto left = creature_footprints::FootOf(feet.left);
	const auto& foot = creature_footprints::LeftIsLower(right, left) ? left : right;
	if (foot.position.y >= k_ScareFootHeight)
	{
		return std::nullopt;
	}
	return foot.position;
}

std::optional<glm::vec3> StepScare(creature_audio::Layers& last, const creature_audio::Layers& current, float turnMs,
                                   const creature_audio::InfoOf& infoOf, const std::optional<creature_footprints::Feet>& feet)
{
	const auto fired = creature_audio::FrameEvents(last, current, turnMs, infoOf);
	if (!feet.has_value())
	{
		return std::nullopt;
	}
	return ScarePoint(fired, *feet);
}

} // namespace openblack::ecs::creature_turn_steps
