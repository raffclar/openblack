/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureFart.h"

using namespace openblack;

bool creature_fart::PlaysSquatLoop(const creature_layers::BodyAction& body)
{
	using creature_layers::BodyAction;
	return body.kind == BodyAction::Kind::Sequence && body.phase == BodyAction::Phase::Loop &&
	       body.animations[1] == creature_layers::animations::k_Poo;
}

bool creature_fart::CountTurn(uint32_t& turns)
{
	++turns;
	if (turns < k_TurnsPerBurst)
	{
		return false;
	}
	turns = 0;
	return true;
}

std::array<glm::vec3, creature_fart::k_Puffs> creature_fart::PuffVelocities(glm::vec2 ahead, float shownSize,
                                                                            const FloatRandom& random)
{
	const float spread = shownSize * k_CloudSpeed;
	const float half = spread * 0.5f;
	const glm::vec2 back = -ahead * spread;
	std::array<glm::vec3, k_Puffs> velocities {};
	for (auto& velocity : velocities)
	{
		const float z = random(spread) - half;
		const float y = random(spread) - half;
		const float x = random(spread) - half;
		velocity = glm::vec3(x + back.x, y, z + back.y);
	}
	return velocities;
}
