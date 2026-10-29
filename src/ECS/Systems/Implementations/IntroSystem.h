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
	void ProcessTurn(uint32_t milliseconds) override;
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

	/// The frames make the hand in the drawn registry if it is not there, and show it with a clip from its start
	void MakeHand(std::string_view clipFile);
	/// The frames do the requests of the scripts for the hand
	void DoHandRequests();
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
	/// The hand in the drawn registry, while it shows
	entt::entity _hand {entt::null};
	/// The scripts ask for the hand that sets the boy down. The next frame makes it.
	bool _setDownWanted {false};
	/// The scripts tell the hand to let the boy go. The next frame moves its clip to the end.
	bool _letGoWanted {false};
	bool _handPlaying {false};
	bool _holdingBoy {false};
	/// Set once the light has gone, when the lifting clip may play
	int32_t _liftGo {0};
	/// The light and the lifting clip as the turns move them on, from the fall of the light until the hand stands ready to
	/// set the boy down. Once the clip comes round, it stays so.
	std::optional<intro_rules::LiftClock> _liftClock;
	bool _liftFinished {false};
	std::optional<glm::vec3> _grip;
};

} // namespace openblack::ecs::systems
