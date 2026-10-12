/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <memory>
#include <optional>

#include <glm/vec3.hpp>

#include "CameraModel.h"
#include "CameraZoomer.h"

/// Where the camera goes while the hand is held to a creature: in front of it, looking at its middle from one and a half
/// of its heights away and a little above, gliding there over 0.7 seconds and following it as it moves and turns.
/// Letting go gives the camera back to the camera it took over from.
namespace openblack::creature_interact
{
/// The camera glides to its place in this long, setting off afresh each frame
constexpr float k_EaseSeconds = 0.7f;
/// The camera looks at this share of the creature's height up its middle
constexpr float k_FocusHeightShare = 0.5f;
/// It stands this many of the creature's heights from where it looks
constexpr float k_DistanceHeights = 1.5f;
/// Its direction from the creature, in the creature's own space (in front of it is -z): 30 units ahead and 15 up, less
/// 0.7 of the creature's height, so it looks up at a tall creature and down at a small one
constexpr float k_Ahead = 30.0f;
constexpr float k_Up = 15.0f;
constexpr float k_UpLessPerHeight = 0.7f;

struct View
{
	glm::vec3 origin;
	glm::vec3 focus;
};
/// The camera's place for a creature standing at a position, so tall, turned about the vertical by a heading as its body
/// is drawn (the heading it is turning to while it turns)
[[nodiscard]] View ViewOf(const glm::vec3& position, float height, float heading);

/// Given back, the player's camera flies back to where it was; it wooshes when it starts further than this from there
constexpr float k_WooshDistance = 150.0f;
[[nodiscard]] bool WooshesFlyingBack(const glm::vec3& from, const glm::vec3& to);
} // namespace openblack::creature_interact

namespace openblack
{

/// The camera while the hand is held to a creature (see creature_interact). It keeps the camera it took over from, to
/// give it back.
class CreatureInteractCameraModel final: public CameraModel
{
public:
	/// Starts from where the camera is
	CreatureInteractCameraModel(glm::vec3 origin, glm::vec3 focus);

	/// Where the creature now is, how tall, and its heading; none once the creature is gone, when the camera glides on to
	/// where it was going and stays
	void SetTarget(std::optional<creature_interact::View> view) { _target = view; }

	void SetPlayerModel(std::unique_ptr<CameraModel> model) { _playerModel = std::move(model); }
	[[nodiscard]] std::unique_ptr<CameraModel> TakePlayerModel() { return std::move(_playerModel); }
	[[nodiscard]] const CameraModel* GetPlayerModel() const { return _playerModel.get(); }

	std::optional<CameraInterpolationUpdateInfo> Update(std::chrono::microseconds dt, const Camera& camera) override;
	/// The player's controls don't move it
	void HandleActions(std::chrono::microseconds /*dt*/) override {}
	void SetFlight(glm::vec3 origin, glm::vec3 focus) override;
	[[nodiscard]] glm::vec3 GetTargetOrigin() const override;
	[[nodiscard]] glm::vec3 GetTargetFocus() const override;
	[[nodiscard]] std::chrono::seconds GetIdleTime() const override { return std::chrono::seconds::zero(); }

private:
	using Zoomers = std::array<camera::Zoomer, 3>;
	Zoomers _origin;
	Zoomers _focus;
	std::optional<creature_interact::View> _target;
	std::unique_ptr<CameraModel> _playerModel;
};

} // namespace openblack
