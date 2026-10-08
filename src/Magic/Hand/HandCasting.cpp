/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandCasting.h"

#include <cstdio>
#include <cstdlib>

#include <glm/geometric.hpp>

#include "Camera/Camera.h"
#include "ECS/Systems/HandMagicStateInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "HandMagicFX.h"
#include "Locator.h"
#include "Magic/Gestures/GestureDebugHooks.h"
#include "Magic/Gestures/GestureInput.h"
#include "Magic/Gestures/PowerUpSystem.h"
#include "Particles/Utility.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
struct HandCastingState
{
	/// The last frame's game time, for the per-turn ProcessPowerUpSystem
	float lastFrameSeconds {0.0f};
};

/// This module's state (Locator::handMagicState)
HandCastingState& Casting()
{
	if (!Locator::handMagicState::has_value())
	{
		std::fputs("magic::hand_casting: no hand magic state in the locator (Locator::handMagicState)\n", stderr);
		std::abort();
	}
	return Locator::handMagicState::value().Get<HandCastingState>();
}
} // namespace

void hand_casting::OnLoadMap()
{
	gestures::Reset();
	gestures::ResetDebugHooks();
	hand_fx::Reset();
	psys::utility::Reset();
	Casting().lastFrameSeconds = 0.0f;
}

void hand_casting::ProcessTurn()
{
	gestures::ProcessPowerUpSystem(Casting().lastFrameSeconds);
}

void hand_casting::Update(float seconds)
{
	Casting().lastFrameSeconds = seconds;
	// the mouse messages and the test strokes, then the interface action's ProcessPowerUpSystem.
	// (not ported) the original runs the interface action once per queued button message and once with none, so a
	// frame with two button edges runs it twice; a hand demo replays its records the same way.
	// openblack reads the buttons once a frame
	gestures::RunDebugHooks(seconds);
	gestures::sampling::Update(seconds);
	gestures::ProcessPowerUpSystem(seconds);
	// the hand's draw: the hand effects and the spell in the hand (game time in ms)
	hand_fx::Update(seconds);
	hand_fx::UpdateInHandEffect(seconds * 1000.0f);
	// the utility effect's trail at the hand, magnitude handScale x f(camera distance to the hand)
	if (Locator::handSystem::has_value())
	{
		const auto& hand = Locator::handSystem::value();
		const glm::vec3 position(hand.GetHandMatrix()[3]);
		const float distance =
		    Locator::camera::has_value() ? glm::distance(Locator::camera::value().GetOrigin(), position) : 0.0f;
		psys::utility::Update(seconds, position, hand.GetHandScale(), distance);
	}
}
