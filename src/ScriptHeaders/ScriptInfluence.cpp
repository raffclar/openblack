/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptInfluence.h"

#include <algorithm>

namespace openblack::script::influence
{

float Answer(std::optional<float> influence, bool raw, std::span<const float> alliesInfluence)
{
	if (!influence.has_value())
	{
		return 0.0f;
	}
	if (raw || *influence > 0.0f)
	{
		return *influence;
	}
	// Lent by the first ally with some there; without one the answer is none, even where the player's own is below none
	const auto lent = std::ranges::find_if(alliesInfluence, [](float ally) { return ally > 0.0f; });
	return lent != alliesInfluence.end() ? *lent : 0.0f;
}

} // namespace openblack::script::influence
