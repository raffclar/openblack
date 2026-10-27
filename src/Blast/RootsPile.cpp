/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RootsPile.h"

#include "3D/MapCoords.h"

using namespace openblack;

float roots_pile::TreeHoleScale(const glm::vec3& treeHalfSize, float treeScale)
{
	// The game adds the depth to the width, then multiplies by the size and the share, with a float for each step
	const float across = treeHalfSize.z + treeHalfSize.x;
	const float sized = across * treeScale;
	return sized * k_TreeHoleShare;
}

bool roots_pile::Age(Pile& pile, int32_t frameMilliseconds)
{
	if (pile.millisecondsLeft <= k_FadeMilliseconds)
	{
		const float alpha = static_cast<float>(pile.millisecondsLeft) * k_AlphaPerMillisecond;
		pile.alpha = static_cast<uint8_t>(map_coords::FtoL(alpha) & 0xFF);
	}
	pile.millisecondsLeft -= frameMilliseconds;
	return pile.millisecondsLeft > 0;
}
