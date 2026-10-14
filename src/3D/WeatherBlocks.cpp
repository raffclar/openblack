/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WeatherBlocks.h"

#include <algorithm>

#include <LNDFile.h>
#include <glm/geometric.hpp>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Scenery/ObjectDrawList.h"

using namespace openblack;

namespace
{
/// A land block's side
constexpr float k_BlockSize = 160.0f;

/// The highest altitude of the cells of a block, as the land had them when it loaded
uint8_t HighestAltitude(const lnd::LNDBlock& block)
{
	return std::ranges::max(block.cells, {}, &lnd::LNDCell::altitude).altitude;
}
} // namespace

std::vector<glm::vec2> weather_blocks::InView(const LandIslandInterface& island, const rain::View& view, bool reflected)
{
	std::vector<glm::vec2> corners;
	const glm::vec2 eye {view.eye.x, view.eye.z};
	for (const auto& block : island.GetBlocks())
	{
		const auto corner = block.GetMapPosition();
		// A block that is further than the reach across the ground is also further than the reach from the eye
		const auto& lnd = block.GetLndBlock();
		if (lnd == nullptr || !(glm::distance(corner + glm::vec2(k_BlockSize * 0.5f), eye) < rain::k_BlockReach))
		{
			continue;
		}
		const object_draw_list::Block box {.corner = corner, .highestAltitude = HighestAltitude(*lnd), .reflected = reflected};
		if (object_draw_list::BlockInView(view.viewProjection, view.nearPlane, box) &&
		    object_draw_list::BlockDistance(view.eye, box) < rain::k_BlockReach)
		{
			corners.push_back(corner);
		}
	}
	return corners;
}
