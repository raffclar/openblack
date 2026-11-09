/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorldCamera.h"

#include "3D/TempleInteriorInterface.h"
#include "Camera.h"
#include "DefaultWorldCameraModel.h"
#include "Locator.h"

namespace openblack::world_camera
{

Camera* BehindTemple()
{
	return Locator::temple::has_value() ? Locator::temple::value().GetWorldCamera() : nullptr;
}

Camera& Get()
{
	if (auto* behind = BehindTemple(); behind != nullptr)
	{
		return *behind;
	}
	return Locator::camera::value();
}

void EndFrameControls()
{
	auto* behind = BehindTemple();
	if (behind == nullptr)
	{
		return;
	}
	if (auto* world = dynamic_cast<DefaultWorldCameraModel*>(&behind->GetModel()); world != nullptr)
	{
		world->SetInputsOff(!TakesControls(true));
	}
}

} // namespace openblack::world_camera
