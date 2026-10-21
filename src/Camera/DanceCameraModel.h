/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "CameraModel.h"

namespace openblack
{

/// The camera of a dance. A dance asks for it when the hand stays near the dance for a time. The camera holds its view:
/// the last move of the camera ends, and then the camera does not move. The player's controls do not move it. A grip of
/// the land ends it, and it also ends when its dance goes. It keeps the camera that it took over from, to give it back.
class DanceCameraModel final: public CameraModel
{
public:
	/// It holds the view that the camera goes to now
	DanceCameraModel(entt::entity dance, glm::vec3 origin, glm::vec3 focus);

	[[nodiscard]] entt::entity GetDance() const { return _dance; }
	/// A grip of the land ends the camera. The camera stays until the next check of the camera in a game turn.
	void End() { _ended = true; }
	[[nodiscard]] bool Ended() const { return _ended; }

	void SetPlayerModel(std::unique_ptr<CameraModel> model) { _playerModel = std::move(model); }
	[[nodiscard]] std::unique_ptr<CameraModel> TakePlayerModel() { return std::move(_playerModel); }
	[[nodiscard]] const CameraModel* GetPlayerModel() const { return _playerModel.get(); }

	/// It gives the camera no new move
	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) override;
	/// The player's controls do not move it
	void HandleActions(std::chrono::microseconds /*dt*/) override {}
	/// It holds its view, so a flight does not move it
	void SetFlight(glm::vec3 /*origin*/, glm::vec3 /*focus*/, float /*rise*/) override {}
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return _origin; }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return _focus; }

private:
	entt::entity _dance;
	glm::vec3 _origin;
	glm::vec3 _focus;
	bool _ended {false};
	std::unique_ptr<CameraModel> _playerModel;
};

} // namespace openblack
