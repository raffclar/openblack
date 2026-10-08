/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/ScriptControlSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class ScriptControlSystem final: public ScriptControlSystemInterface
{
public:
	bool StartCameraControl(Camera& camera, const CameraRequest& request, GroundHeight groundAt) override;
	bool EndCameraControl(Camera& camera, uint32_t task) override;
	[[nodiscard]] uint32_t GetCameraOwner() const override { return _cameraOwner; }
	[[nodiscard]] ScriptCameraModel* GetScriptCamera(Camera& camera) const override;

	void StartGameSpeed(uint32_t task) override;
	bool EndGameSpeed(uint32_t task) override;
	[[nodiscard]] bool MaySetGameSpeed(uint32_t task) const override { return _gameSpeedOwner == task; }

	Released TaskStopped(Camera& camera, uint32_t task) override;

	void Reset() override;

private:
	/// The camera goes back to the player, from where it is
	void ReleaseCamera(Camera& camera);

	uint32_t _cameraOwner {0};
	uint32_t _gameSpeedOwner {0};
};

} // namespace openblack::ecs::systems
