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
#include "ECS/Systems/CameraPathSystemInterface.h"
#include "FightCameraModel.h"
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

DefaultWorldCameraModel* PlayersWorldModel(CameraModel& model)
{
	if (auto* fight = dynamic_cast<FightCameraModel*>(&model); fight != nullptr)
	{
		return fight->GetPlayerModel() != nullptr ? dynamic_cast<DefaultWorldCameraModel*>(fight->GetPlayerModel()) : nullptr;
	}
	return dynamic_cast<DefaultWorldCameraModel*>(&model);
}

void EndFrameControls(bool frontEndBox)
{
	// The camera path is a model of its own: the player's camera is then not the current model, and keeps its state
	if (Locator::cameraPathSystem::has_value() && Locator::cameraPathSystem::value().HoldsCamera())
	{
		return;
	}
	auto* behind = BehindTemple();
	if (behind == nullptr && !Locator::camera::has_value())
	{
		return;
	}
	auto& camera = behind != nullptr ? *behind : Locator::camera::value();
	if (auto* world = PlayersWorldModel(camera.GetModel()); world != nullptr)
	{
		world->SetInputsOff(InputsOff(behind != nullptr, frontEndBox));
	}
}

} // namespace openblack::world_camera
