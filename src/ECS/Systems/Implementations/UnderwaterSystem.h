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
#include <optional>
#include <vector>

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
	[[nodiscard]] float GetSeaLevel() const override { return _seaLevel; }
	[[nodiscard]] const graphics::underwater::WaveState& GetWaves() const override { return _waves; }
	[[nodiscard]] float GetSeconds() const override { return _seconds; }
	[[nodiscard]] std::span<const glm::vec3> GetFloaters() const override { return _floaters; }
	[[nodiscard]] glm::mat4 FloatOnWaves(const glm::mat4& model) const override;
	void StartDebugStorm() override;
	void DropDebugFloaters() override;
	[[nodiscard]] const seabed::Seabed& GetSeabed() const override { return _seabed; }
	[[nodiscard]] uint32_t GetSeabedRevision() const override { return _revision; }
	[[nodiscard]] std::span<const seabed::ReefPlant> GetReef() const override { return _reef; }
	[[nodiscard]] std::span<const graphics::underwater::Waterspout> GetWaterspouts() const override { return _waterspouts; }
	[[nodiscard]] const std::optional<graphics::underwater::RogueWave>& GetRogueWave() const override { return _rogue; }
	[[nodiscard]] const graphics::underwater::SeaStrike& GetSeaStrike() const override { return _strike; }
	[[nodiscard]] glm::vec2 GetSeaFlash() const override { return _seaFlash; }
	[[nodiscard]] const graphics::underwater::CurrentState& GetCurrent() const override { return _current; }
	[[nodiscard]] float FloorAt(glm::vec2 point, float landHeight) const override;

private:
	/// The sand texture of the sea bed, once
	static void LoadSand();
	/// The tide, the storm and the waves, and the things that float
	void UpdateSea(float frameSeconds);
	/// The rogue wave and the lightning on the sea. `seaDistance` gives the distance from the coast at a point.
	void UpdateStorm(float frameSeconds, const std::function<float(glm::vec2)>& seaDistance);
	/// Whether a thing whose physics body is drawn here floats on the sea
	[[nodiscard]] bool Floats(const glm::vec3& origin) const;

	bool _enabled {true};
	float _eyeDepth {0.0f};
	bool _eyeUnder {false};
	/// How much the land's sounds and the music are turned down now, eased towards its target
	float _duck {1.0f};
	float _seaLevel {0.0f};
	float _seconds {0.0f};
	float _storm {0.0f};
	/// The shore wave's phase, which grows each frame (see underwater::AdvanceShorePhase)
	float _shorePhase {0.0f};
	float _windAngle {0.0f};
	graphics::underwater::WaveState _waves;
	std::vector<glm::vec3> _floaters;
	std::vector<seabed::ReefPlant> _reef;
	std::vector<graphics::underwater::Waterspout> _waterspouts;
	/// The rogue wave now, its own clock, and the point it comes near; the point follows the camera while no wave lives
	std::optional<graphics::underwater::RogueWave> _rogue;
	float _rogueSeconds {0.0f};
	glm::vec2 _rogueCentre {0.0f};
	/// The debug storm makes the first chance of a rogue wave certain
	bool _rogueCertain {false};
	graphics::underwater::SeaStrike _strike;
	glm::vec2 _seaFlash {0.0f};
	graphics::underwater::CurrentState _current;
	float _tideRate {0.0f};
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
