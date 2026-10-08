/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The interface's memory of the last thing tapped or clicked, the one slot that GAME_THING_CLICKED and POSITION_CLICKED
// read: the object with its turn, the land tap with its turn. See docs/bw1-notes/hand-and-interface.md.

#define LOCATOR_IMPLEMENTATIONS

#include "Common/GUtilsDistance.h"
#include "ECS/Registry.h"
#include "GameClock.h"
#include "HandSystem.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// (turn - stored turn) x msPerTurn x 0.001 > 15.0 s of game time
bool Expired(uint32_t storedTurn)
{
	// the turns times the ms per turn, then 0.001: 150 turns give 15.0000010 > 15, not 15 x 0.1 = 15.0
	const float seconds =
	    static_cast<float>(game_clock::Turn() - storedTurn) * static_cast<float>(game_clock::MsPerTurn()) * 0.001f;
	return seconds > 15.0f;
}
} // namespace

entt::entity HandSystem::GetClickedObject() const noexcept
{
	// null when nothing is stored or the object no longer exists
	if (_clickedObject == entt::null || !Locator::entitiesRegistry::value().Valid(_clickedObject))
	{
		return entt::null;
	}
	return _clickedObject;
}

void HandSystem::ClearClicked() noexcept
{
	// the object goes; its turn stays
	_clickedObject = entt::null;
}

void HandSystem::RememberTapped(entt::entity object) noexcept
{
	// the object (null clears it) and the game turn
	_clickedObject = object;
	_clickedTurn = game_clock::Turn();
}

bool HandSystem::PositionClicked(const glm::vec3& position, float radius) const noexcept
{
	// the 2D distance in metres from the land tap, at most the radius
	return gutils::GetDistanceInMetres(_clickedPosition, position) <= radius;
}

void HandSystem::ClearClickedPosition() noexcept
{
	_clickedPosition = glm::vec3(0.0f);
}

void HandSystem::UpdateTapMemory(bool actionReleased) noexcept
{
	// (before the action states) the action button released in action state 0 with nothing in the hand and the game
	// not paused remembers the collided object, else the land point with its turn. (not ported) a Reward under the
	// leash is not remembered. Otherwise the two 15 s expiries
	const bool idle = !_held && !_tug && !_pickSource && !_releaseArmed && !_pendingPick && !_gripPoint;
	if (actionReleased && idle && !game_clock::IsPaused())
	{
		const auto object = _hovered ? *_hovered : (_cursorObject ? *_cursorObject : entt::null);
		if (object != entt::null)
		{
			RememberTapped(object);
			return;
		}
		_clickedObject = entt::null;
		_clickedPosition = _interactionPoint.value_or(glm::vec3(0.0f));
		_clickedPositionTurn = game_clock::Turn();
		return;
	}
	if (Expired(_clickedPositionTurn))
	{
		_clickedPosition = glm::vec3(0.0f);
	}
	if (_clickedObject != entt::null && Expired(_clickedTurn))
	{
		_clickedObject = entt::null;
	}
}
