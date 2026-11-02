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

#include <entt/entity/entity.hpp>

#include "3D/HandAnimation.h"

/// When the drawn hand starts a new blend from how it looked before. The hand has a mode for each thing that it does.
/// Each change of mode starts a blend. In some modes, a change of the animation also starts a blend. A change of the
/// thing that the hand holds also starts a blend.
namespace openblack::hand_blend
{

/// What the hand does, for its drawn pose
enum class HandMode : uint8_t
{
	/// The hand is not drawn
	Hidden,
	/// The hand moves over the world with nothing in it
	Normal,
	/// The hand turns, pulls or moves the camera
	Camera,
	/// The hand pulls a thing free of the land
	Pulling,
	/// The hand holds a thing. This includes a handful that it scoops from a pile.
	Holding,
	/// The hand holds the totem of a town
	Totem,
	/// The hand touches a creature
	Creature,
	/// The hand holds the seed of a miracle
	Seed,
	/// The hand knocks on a building
	Knocking,
	/// The hand is in the rooms of the temple
	Temple,
};

/// What the game knows about the hand in this frame
struct ModeInputs
{
	bool shown {true};
	/// The player is inside the temple
	bool insideTemple {false};
	/// The hand holds a thing or a seed
	bool holds {false};
	/// What the hand holds is the seed of a miracle
	bool holdsSeed {false};
	/// The hand pulls a thing free of the land
	bool pulls {false};
	bool knocks {false};
	bool gripsTotem {false};
	bool touchesCreature {false};
	/// The camera state of the hand is on
	bool movesCamera {false};
};

/// The mode of the hand. Inside the temple, the hand ignores the other inputs. A hand that holds or pulls a thing
/// ignores the inputs after these. A hand that knocks ignores the totem, the creature and the camera.
[[nodiscard]] HandMode ModeOf(const ModeInputs& inputs);

/// Whether the mode of the hand lets the camera's icons by the hand show. The empty hand and the camera mode let them
/// show. A hand that holds a thing or a seed also lets them show, but not while it scoops from a pile, a field or a
/// fish farm that it is locked onto. The other modes do not let them show.
[[nodiscard]] constexpr bool AllowsCameraIcons(HandMode mode, bool scoopsLockedThing)
{
	switch (mode)
	{
	case HandMode::Normal:
	case HandMode::Camera:
		return true;
	case HandMode::Holding:
	case HandMode::Seed:
		return !scoopsLockedThing;
	default:
		return false;
	}
}

/// What the hand does in this frame, for the changes that start a blend
struct Frame
{
	HandMode mode {HandMode::Normal};
	/// The animation that the hand plays in the normal, camera or creature mode
	HandAnimation::Cycle cycle {HandAnimation::Cycle::Wiggle};
	/// The thing or seed that the hand holds
	std::optional<entt::entity> held;
	/// No thing, bubble or leash is below the hand
	bool nothingUnderHand {true};
};

/// The changes in one frame
struct Changes
{
	/// How many times the blend starts again in this frame. Each start keeps the last drawn pose and puts back the pose
	/// that the blend kept before.
	uint8_t count {0};
	/// The pose of this frame goes in place of the kept pose, and not in place of the drawn pose. This occurs when the
	/// camera mode changes its animation after the frame chose where its pose goes.
	bool poseIntoKept {false};
	/// The mode that the hand left in this frame, when it changed from one mode to a different mode
	std::optional<HandMode> left;
};

/// Whether the hand resets its eases for the normal mode after it places itself in the frame of the change. When the
/// hand goes into the normal mode, its hover distance starts again from where the hand is, with no speed. Its up
/// starts again straight up, and the hand ignores the last sparkle of a fight. The frame decides these modes only after
/// it placed the hand, so the reset applies from the next frame. The hand that leaves the camera mode, a knock or the
/// temple does these resets before it places itself, because the frame knows these modes first.
[[nodiscard]] constexpr bool ResetsAfterPlacing(HandMode left, HandMode entered)
{
	if (entered != HandMode::Normal)
	{
		return false;
	}
	switch (left)
	{
	case HandMode::Hidden:
	case HandMode::Pulling:
	case HandMode::Holding:
	case HandMode::Totem:
	case HandMode::Creature:
	case HandMode::Seed:
		return true;
	default:
		return false;
	}
}

/// Finds the changes of the hand that start a blend, frame by frame
class ChangeTracker
{
public:
	/// The changes in this frame
	Changes Step(const Frame& frame);

private:
	void Enter(HandMode mode);

	std::optional<HandMode> _mode;
	std::optional<entt::entity> _held;
	/// The last animation of the normal mode. It is empty when the hand enters the mode.
	std::optional<HandAnimation::Cycle> _normalCycle;
	HandAnimation::Cycle _cameraCycle {HandAnimation::Cycle::Wiggle};
	HandAnimation::Cycle _creatureCycle {HandAnimation::Cycle::Tickle};
};

} // namespace openblack::hand_blend
