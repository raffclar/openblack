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
	/// Takes over from the player's camera, flying to a view of the fight through a point on the way
	FightCameraModel(std::unique_ptr<CameraModel> playerModel, const fight_view::View& start);

	/// Takes control again after the hand lets go of a creature: from where the camera now is and how fast it moves, it
	/// flies back to where this view last was, and then eases to the fight
	void Return(const fight_view::View& from, const fight_view::View& speed);

	/// Where the fighters now are, the first the one that made the arena
	void SetFighters(const fight_view::Fighter& first, const fight_view::Fighter& second);
	/// Where the camera was when its last step started
	[[nodiscard]] const fight_view::View& GetShown() const { return _chase.GetHeld(); }
	/// Whether the player has zoomed out of the fight
	[[nodiscard]] bool ZoomedOut() const { return _zoomedOut; }
	/// Whether the player held the land in the last step, and the seconds since the last flight to the view started, or
	/// started its last part
	[[nodiscard]] bool LandHeld() const { return _landHeld; }
	[[nodiscard]] float GetSecondsSinceFlight() const { return _sinceFlight; }
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
	fight_view::Chase _chase;
	/// The view the fight wants
	fight_view::View _view;
	std::optional<fight_view::Fighter> _first;
	std::optional<fight_view::Fighter> _second;
	fight_view::Input _input;
	CameraModel::Asked _asked;
	float _sinceFlight {0.0f};
	/// While the player holds the land, the player's camera moves it; the view then goes on from where it is
	bool _landHeld {false};
	/// The first step starts the flight to the fight from where the camera is
	bool _started {false};
	bool _zoomedOut {false};
};

} // namespace openblack
