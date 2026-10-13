/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>

#include <entt/entity/fwd.hpp>

namespace openblack::graphics
{
class L3DMesh;
}

namespace openblack::ecs::components
{
struct Transform;
}

namespace openblack::ecs::systems
{

/// The smoke from the homes' chimneys (components::ChimneySmoke), lit while someone is home, and the air the hand
/// stirs as it passes
class ChimneySmokeSystemInterface
{
public:
	virtual ~ChimneySmokeSystemInterface() = default;

	/// Gives a home whose mesh has a chimney its smoke, grey from a workshop
	virtual void Attach(entt::entity abode, const graphics::L3DMesh& mesh, const components::Transform& transform,
	                    bool workshop) = 0;
	/// Once a game turn: how fast the hand is moving
	virtual void ProcessTurn() = 0;
	/// Once a frame, with the real time and the game time of the frame: each smoke lights or stops with its home, and
	/// its puffs move. They move at most 60 times a second. Each step uses all the game time since the last step
	virtual void Update(std::chrono::milliseconds frameRealTime, std::chrono::milliseconds frameGameTime) = 0;
};

} // namespace openblack::ecs::systems
