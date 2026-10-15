/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AbodeNeglectRules.h"

namespace openblack::ecs::abode_neglect
{

bool CountsAsEmpty(const NeglectFacts& facts)
{
	// A building with no places of a kind counts as full of that kind
	if (facts.maxAdults == 0 || facts.maxChildren == 0)
	{
		return false;
	}
	// The child share divides whole numbers, thus it is zero until each child place is full
	return facts.adults == 0 && facts.children / facts.maxChildren == 0;
}

NeglectStep Examine(float timer, const NeglectFacts& facts)
{
	if (!CountsAsEmpty(facts) || !facts.built || facts.inScript || facts.townUninhabitable)
	{
		return {.timer = timer, .losesLife = false};
	}
	const float next = timer + k_NeglectStep;
	if (next >= 1.0f)
	{
		return {.timer = 0.0f, .losesLife = true};
	}
	return {.timer = next, .losesLife = false};
}

} // namespace openblack::ecs::abode_neglect
