/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include "ECS/Systems/UnderwaterSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class UnderwaterSystem final: public UnderwaterSystemInterface
{
public:
	[[nodiscard]] bool IsEnabled() const override { return _enabled; }
	void SetEnabled(bool enabled) override { _enabled = enabled; }
	[[nodiscard]] graphics::underwater::Effect GetEffectOff() const override { return _effectOff; }
	void CycleEffectOff() override;
	void StartTour(graphics::underwater::Tour tour) override
	{
		_tour = tour;
		_tourSeconds = 0.0f;
	}
	void Update(std::chrono::milliseconds frameRealTime) override;
	[[nodiscard]] float GetEyeDepth() const override { return _eyeDepth; }
	[[nodiscard]] bool IsEyeUnder() const override { return _eyeUnder; }
	[[nodiscard]] const seabed::Seabed& GetSeabed() const override { return _seabed; }
	[[nodiscard]] uint32_t GetSeabedRevision() const override { return _revision; }
	[[nodiscard]] float FloorAt(glm::vec2 point, float landHeight) const override;

private:
	/// The sand texture of the sea bed, once
	static void LoadSand();

	bool _enabled {true};
	float _eyeDepth {0.0f};
	bool _eyeUnder {false};
	/// How much the land's sounds and the music are turned down now, eased towards its target
	float _duck {1.0f};
	/// How far the debug tour has gone, in seconds; none while it does not run
	std::optional<float> _tourSeconds;
	graphics::underwater::Tour _tour {graphics::underwater::Tour::Shore};
	graphics::underwater::Effect _effectOff {graphics::underwater::Effect::None};
	seabed::Seabed _seabed;
	uint32_t _revision {0};
	/// The land the sea bed was made for: its height map, as each land makes its own
	const void* _builtFor {nullptr};
};

} // namespace openblack::ecs::systems
