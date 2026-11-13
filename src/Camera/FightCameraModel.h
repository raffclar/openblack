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
#include <optional>

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
	[[nodiscard]] bool LandGripped() const { return _asked.landGripped; }
	[[nodiscard]] float GetSecondsSinceFlight() const { return _sinceFlight; }
	/// Whether the player moved the camera in the last frame, as the player's camera measured it
	[[nodiscard]] bool MovedByPlayer() const { return _asked.movedByPlayer; }
	/// The player's own camera, which it hands back
	void SetPlayerModel(std::unique_ptr<CameraModel> model) { _playerModel = std::move(model); }
	[[nodiscard]] std::unique_ptr<CameraModel> TakePlayerModel() { return std::move(_playerModel); }
	[[nodiscard]] CameraModel* GetPlayerModel() { return _playerModel.get(); }
	/// Starts again from where the camera is, as when the player comes out of the temple with the fight on. The view
	/// stays on the fight:
	/// - The next step puts the heading and the point of the view where they must be, at once.
	/// - A flight stops, and the count of seconds since the flight starts again from zero.
	/// - The player's own camera starts again (see DefaultWorldCameraModel::StartAgain).
	void StartAgain();

	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) override;
	void HandleActions(std::chrono::microseconds dt) override;
	/// The player's camera reads its keys, for the next frame
	void HandleKeys(std::chrono::microseconds dt) override;
	/// A flight (such as a double tap of the temple key) goes through a point on the way, then the view of the fight takes
	/// the camera back
	void SetFlight(glm::vec3 origin, glm::vec3 focus, float rise) override;
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override { return _view.origin; }
	[[nodiscard]] glm::vec3 GetTargetFocus() const override { return _view.focus; }
	/// A step that follows the fighters removes the point that a script gave the player's camera to turn about. A step
	/// while the player holds the land does not.
	[[nodiscard]] bool EndsForcedTurnPoint() const override { return _followedFighters; }

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
	/// In a step in which the movement keys move the camera, the player's camera moves it; the view then goes on from
	/// where it is
	bool _keysMoved {false};
	/// The first step starts the flight to the fight from where the camera is
	bool _started {false};
	/// The last step followed the fighters
	bool _followedFighters {false};
	/// A flight asked for before the first step, which the first step starts in place of the flight to the fight
	struct FirstFlight
	{
		fight_view::View to;
		float rise;
	};
	std::optional<FirstFlight> _firstFlight;
	bool _zoomedOut {false};
};

} // namespace openblack
