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

#include <entt/entity/entity.hpp>

namespace openblack::ecs::systems
{

/// Animates the puffs of mist (components::Mist)
/// The frames of the mists. Only a computer that draws the world has this system.
class MistSystemInterface
{
public:
	virtual ~MistSystemInterface() = default;
	/// Moves every mist's animation on by the game time that has passed, which stops while the game is paused
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;
	/// Starts a script's fade of a mist: its size goes from `startSize` to `endSize` and its alpha (0 to 255) from
	/// `startAlpha` to `endAlpha` over `seconds`. It takes the start size and the start alpha at once.
	virtual void SetFade(entt::entity mist, float startSize, float endSize, float startAlpha, float endAlpha,
	                     float seconds) = 0;
	/// Moves each fading mist one step on: once each game turn
	virtual void ProcessTurn() = 0;
};

} // namespace openblack::ecs::systems
