/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "YardCopyRules.h"

#include <algorithm>

#include "Common/X87.h"

namespace openblack::ecs::yard_copy
{

uint8_t Alpha(int32_t clockMs, int32_t landAlpha)
{
	// The game finds the angle of the clock, adds 1 to its cosine, and multiplies by a third of the land's alpha and a
	// half. Each step rounds to a float, in the order of the game.
	const float turned = x87::Mul(static_cast<double>(clockMs), k_FullTurn);
	const float angle = x87::Div(turned, static_cast<double>(k_PeriodMs));
	const float wave = x87::Add(x87::Cos(angle), 1.0);
	const float third = x87::Mul(static_cast<double>(landAlpha), k_Third);
	const float pulse = x87::Mul(x87::Mul(third, wave), 0.5);
	return static_cast<uint8_t>(std::clamp(x87::ToInteger(pulse), 0, 255));
}

} // namespace openblack::ecs::yard_copy
