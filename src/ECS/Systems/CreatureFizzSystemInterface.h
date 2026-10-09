/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

namespace openblack::ecs::systems
{

/// Creatures fizzing out of sight and back in through static (components::CreatureFizz, rules in creature_fizz)
class CreatureFizzSystemInterface
{
public:
	virtual ~CreatureFizzSystemInterface() = default;
	/// Sets a creature's fizz going to a target, 0 in full sight to 1 gone, over so many seconds (at once over none).
	/// Going right out of sight sounds the teleport's energise where it stands. One that goes for good is removed from
	/// the world once it has fizzed right out, and nothing changes its fizz again.
	virtual void SetFizz(entt::entity creature, float target, float seconds, bool goesForGood) = 0;
	/// Once a game turn, after the creatures have acted: every fizz moves on
	virtual void ProcessTurn() = 0;
	/// Each frame, by the frame's game time: the static every creature is drawn fizzing through slides across its skin,
	/// and the static its eyes share slides on
	virtual void UpdateFrame(float seconds) = 0;
	/// How far the one static all the creatures' eyes fizz through has slid: from the start of the game, every frame,
	/// whether or not any creature is fizzing
	[[nodiscard]] virtual glm::vec2 EyeStaticScroll() const = 0;
	/// How far a creature has fizzed out of sight, 0 to 1
	[[nodiscard]] virtual float FizzOf(entt::entity creature) const = 0;
};

} // namespace openblack::ecs::systems
