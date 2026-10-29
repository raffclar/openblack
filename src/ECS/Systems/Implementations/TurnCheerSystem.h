/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/TurnCheerSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class TurnCheerSystem final: public TurnCheerSystemInterface
{
public:
	void Start() final
	{
		_turnSpeed = 0.0f;
		help::turn_cheer::Start(_cheering);
	}

	void Stop() final { help::turn_cheer::Stop(_cheering); }

	void MeasureTurn(std::optional<float> turned, float seconds) final
	{
		_turnSpeed = help::turn_cheer::SmoothSpeed(_turnSpeed, turned.value_or(0.0f), seconds);
	}

	[[nodiscard]] std::optional<int32_t> ProcessTurn(uint32_t gameTurn, const help::turn_cheer::Random& random) final
	{
		return help::turn_cheer::ProcessTurn(_cheering, _turnSpeed, gameTurn, random);
	}

	[[nodiscard]] float GetTurnSpeed() const final { return _turnSpeed; }
	[[nodiscard]] const help::turn_cheer::Cheering& GetCheering() const final { return _cheering; }

private:
	/// How fast the player turns the world camera, in radians a second, measured over the last steps of the camera
	float _turnSpeed {0.0f};
	help::turn_cheer::Cheering _cheering;
};

} // namespace openblack::ecs::systems
