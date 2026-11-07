/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LandDetailSystem.h"

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "EngineConfig.h"
#include "Locator.h"
#include "Visual/DetailLevel.h"

using namespace openblack::ecs::systems;
namespace land_detail = openblack::graphics::land_detail;

const land_detail::FrameDetail& LandDetailSystem::For(const Camera& camera)
{
	_places.clear();
	if (Locator::terrainSystem::has_value())
	{
		for (const auto& block : Locator::terrainSystem::value().GetBlocks())
		{
			_places.push_back({.corner = block.GetMapPosition(), .at = block.GetBlockPosition()});
		}
	}
	land_detail::FrameInputs inputs {.origin = camera.GetOrigin(), .forward = camera.GetForward()};
	if (Locator::config::has_value())
	{
		const auto& config = Locator::config::value();
		inputs.lines = config.landDetailLines.value_or(land_detail::For(config.detailLevel));
		inputs.coarseLand = config.detail.coarseLand;
	}
	return _memo.For(inputs, _places);
}
