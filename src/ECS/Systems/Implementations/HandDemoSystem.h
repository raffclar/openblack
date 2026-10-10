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
#include <vector>

#include "ECS/Systems/HandDemoSystemInterface.h"
#include "Hand/HandDemo.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::hnd
{
struct HNDFile;
}

namespace openblack::ecs::systems
{

class HandDemoSystem final: public HandDemoSystemInterface
{
public:
	void Play(std::string_view name, uint32_t task, bool pauseOnTrigger, bool withoutHandModify) override;
	[[nodiscard]] bool IsPlaying(uint32_t task) const override;
	bool TakeTrigger() override;
	void Update() override;
	void Stop() override;
	void TaskStopped(uint32_t task) override;
	void Reset() override;

	[[nodiscard]] std::optional<CameraPose> GetCamera() const override;
	[[nodiscard]] uint32_t GetHints() const override { return _hints; }
	[[nodiscard]] float GetHintAngle() const override { return _hintAngle; }
	[[nodiscard]] HeldButtons GetHeldButtons() const override { return _held; }
	[[nodiscard]] std::optional<Status> GetStatus() const override;

private:
	/// The records played this go, through the player's interface
	void Apply(const std::vector<hand_demo::Played>& played);

	std::shared_ptr<const hnd::HNDFile> _file;
	std::unique_ptr<hand_demo::Playback> _playback;
	std::string _name;
	/// The task that started it
	uint32_t _task {0};
	bool _pauseOnTrigger {false};
	bool _triggerReached {false};
	std::optional<CameraPose> _camera;
	uint32_t _hints {0};
	float _hintAngle {0.0f};
	HeldButtons _held;
};

} // namespace openblack::ecs::systems
