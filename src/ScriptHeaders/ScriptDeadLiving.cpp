/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptDeadLiving.h"

using namespace openblack::script;

bool dead_living::IsDead(const LivingFacts& facts) noexcept
{
	if (facts.kind == LivingFacts::Kind::Other)
	{
		return false;
	}
	if (facts.deathMark || facts.topState == k_DeadState)
	{
		return true;
	}
	if (facts.kind == LivingFacts::Kind::Villager)
	{
		return facts.topState == k_SetDyingState || facts.topState == k_DyingState || facts.finalState == k_DyingState;
	}
	return facts.life == 0.0f;
}
