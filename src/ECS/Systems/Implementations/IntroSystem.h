/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>

#include "ECS/Systems/IntroSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class IntroSystem final: public IntroSystemInterface
{
public:
	void Play(int32_t special) override;
	void ReleaseAll() override;
	[[nodiscard]] bool HasLiftFinished() const override { return _liftFinished; }
	void Update(uint32_t milliseconds) override;
	[[nodiscard]] std::optional<CameraView> GetCameraView() const override;
	[[nodiscard]] const intro_rules::light::Frame* GetLightFrame() const override;
	[[nodiscard]] glm::vec3 GetLightSortPoint() const override;
	[[nodiscard]] State GetState() const override;

private:
	enum class Stage : int32_t
	{
		None = -1,
		Falling = 0,
		SettingDown = 4,
		Lifting = 12,
	};

	/// The hand's entity with its model, a clip and where it stands
	void MakeHand(std::string_view clipFile);
	void PlaceHand(const glm::vec3& position, float yaw);
	void FreeHand();
	/// The hand's clip moved on; true when it came round
	bool AdvanceHand(uint32_t milliseconds);
	/// The hand posed at its clip's time
	void PoseHand();
	/// Where the hand holds the boy now, kept from the last frame it held him
	void UpdateGrip();
	/// Those following the hand drawn in its grip, or where they stand again
	void PlaceFollowers();

	Stage _stage {Stage::None};
	std::optional<intro_rules::light::Light> _light;
	intro_rules::light::Frame _lightFrame;
	bool _lightDrawn {false};
	bool _cameraChasing {false};
	glm::vec3 _cameraOrigin {0.0f};
	entt::entity _hand {entt::null};
	bool _handPlaying {false};
	bool _holdingBoy {false};
	/// Set once the light has gone, when the lifting clip may play
	int32_t _liftGo {0};
	bool _liftFinished {false};
	std::optional<glm::vec3> _grip;
};

} // namespace openblack::ecs::systems
