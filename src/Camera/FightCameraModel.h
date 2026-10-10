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

#include "CameraModel.h"
#include "FightView.h"

namespace openblack
{

/// The camera watching a fight (see fight_view): it flies to the side of the arena, then follows the fighters, which
/// the fight tells it of every frame. It takes over from the player's camera, which still reads the player's turning,
/// tilting and zooming for it, and hands it back afterwards.
class FightCameraModel final: public CameraModel
{
public:
	/// Takes over from the player's camera, flying to a view of the fight
	FightCameraModel(std::unique_ptr<CameraModel> playerModel, const fight_view::View& start);

	/// Where the fighters now are, the first the one that made the arena
	void SetFighters(const fight_view::Fighter& first, const fight_view::Fighter& second);
	/// Whether the player has zoomed out of the fight
	[[nodiscard]] bool ZoomedOut() const { return _zoomedOut; }
	/// The player's own camera, which it hands back
	void SetPlayerModel(std::unique_ptr<CameraModel> model) { _playerModel = std::move(model); }
	[[nodiscard]] std::unique_ptr<CameraModel> TakePlayerModel() { return std::move(_playerModel); }

	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) override;
	void HandleActions(std::chrono::microseconds dt) override;
	void SetFlight(glm::vec3 origin, glm::vec3 focus) override;
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return _view.origin; }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return _view.focus; }
	[[nodiscard]] std::chrono::seconds GetIdleTime() const override { return std::chrono::seconds::zero(); }

private:
	std::unique_ptr<CameraModel> _playerModel;
	fight_view::Tracker _tracker;
	fight_view::View _view;
	std::optional<fight_view::Fighter> _first;
	std::optional<fight_view::Fighter> _second;
	fight_view::Input _input;
	/// The flight to the fight is sent, and how long it has flown
	bool _flightSent {false};
	float _flownSeconds {0.0f};
	bool _zoomedOut {false};
};

} // namespace openblack
