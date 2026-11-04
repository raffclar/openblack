/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <memory>

#include <glm/vec3.hpp>

#include "ECS/Systems/CreatureModeSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack
{
class CameraModel;
class Camera;
class CreatureCameraModel;
} // namespace openblack

namespace openblack::ecs::systems
{

class CreatureModeSystem final: public CreatureModeSystemInterface
{
public:
	CreatureModeSystem();
	~CreatureModeSystem() override;
	CreatureModeSystem(const CreatureModeSystem&) = delete;
	CreatureModeSystem& operator=(const CreatureModeSystem&) = delete;

	void Update(std::chrono::microseconds dt, const Frame& frame) override;
	bool Press(const creature_mode::Press& press) override;
	void PressCreatureKey() override;
	bool Enter(entt::entity creature, creature_follow::StartDistance start) override;
	bool EnterLoaded(entt::entity thing, const creature_follow::Loaded& loaded) override;
	void Leave() override;
	[[nodiscard]] bool IsActive() const override { return _thing.has_value(); }
	[[nodiscard]] std::optional<entt::entity> GetCreature() const override;
	[[nodiscard]] std::optional<entt::entity> GetThing() const override { return _thing; }
	[[nodiscard]] std::optional<entt::entity> PlayersCreature() const override;
	[[nodiscard]] std::optional<creature_follow::View> GetView() const override;
	void HoldKeys(int across, int along, bool shift, bool ctrl, float seconds) override;
	void ClearView() override;

private:
	/// Makes the camera of the thing from the camera now, the place of the thing and its height
	using MakeModel = std::function<std::unique_ptr<CreatureCameraModel>(const Camera& camera, glm::vec3 thing, float height)>;
	/// Locks onto a thing with the camera that the function makes; false when the camera can't be taken now
	bool Take(entt::entity thing, const MakeModel& make);
	[[nodiscard]] bool OwnsCamera() const;
	/// Hands the player's camera back, if the camera is with this mode's
	void ReleaseCamera();
	/// Whether the thing can still be followed: it is there, and nothing else needs the camera
	[[nodiscard]] bool StillValid() const;
	/// Whether the thing is still there
	[[nodiscard]] bool ThingThere() const;

	/// The thing that the camera follows: a creature, or another thing for the follow camera of a saved game
	std::optional<entt::entity> _thing;
	/// The camera's model while locked on, and the player's own, given back on leaving
	CreatureCameraModel* _model {nullptr};
	std::unique_ptr<CameraModel> _playerModel;
	/// Where the player's camera was going when this mode took it. When the thing goes, the player's camera flies
	/// back there.
	glm::vec3 _returnOrigin {0.0f};
	glm::vec3 _returnFocus {0.0f};
	/// Leaving was asked for while another had the camera; it is given back once this mode has it again
	bool _leaving {false};
	/// Whether the hand gripped the land last frame, so that only a fresh grip counts
	bool _wasGripping {false};
	creature_mode::DoubleClicks _doubleClicks;
	/// The keys the testbed holds down
	struct HeldKeys
	{
		int across {0};
		int along {0};
		bool shift {false};
		bool ctrl {false};
		float seconds {0.0f};
	};
	HeldKeys _held;
};

} // namespace openblack::ecs::systems
