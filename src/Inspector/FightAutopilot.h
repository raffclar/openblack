/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Debug/FightAutopilotInterface.h"
#include "Inspector/InputControl.h"

namespace openblack::inspector
{

/// The fight autopilot of the inspector. Once a frame, before the game reads its input, it looks at the fight of the
/// player's creature and gives the next input through the inspector's input, as the input tools do. It uses only what
/// the player sees and does: the pointer, the buttons, the camera and the state of the fight.
class FightAutopilot final: public fight_autopilot::AutopilotInterface
{
public:
	explicit FightAutopilot(InputTargetInterface& input);

	std::string Start(fight_autopilot::Settings settings) override;
	void Stop(std::string_view why) override;
	[[nodiscard]] fight_autopilot::Status GetStatus() const override;
	[[nodiscard]] std::span<const fight_autopilot::LogEntry> GetLog() const override { return _log; }
	void ClearLog() override { _log.clear(); }

	/// Once a frame, before the game reads its input
	void Frame(uint64_t frame);

private:
	enum class Phase : uint8_t
	{
		Waiting,
		/// The double click into the arena, a step each frame
		ShowingFight,
		Aiming,
		Holding,
		Cooling,
	};
	[[nodiscard]] static std::string_view Name(Phase phase);

	void Log(std::string_view event, std::string detail, std::optional<glm::ivec2> pointer = std::nullopt);
	/// Gives one input; logs why when the input fails
	bool Give(const InputEvent& event);
	/// The next step of the double click into the arena of the fight
	void ShowFight(entt::entity own);
	/// The pixel to aim at for the press, and whether the hand points at the right thing now
	[[nodiscard]] std::optional<glm::ivec2> AimPoint(entt::entity own, entt::entity opponent) const;
	[[nodiscard]] bool PickMatches(entt::entity own, entt::entity opponent) const;
	/// The results of the presses, as the fight transcript records them
	void ReadResults(entt::entity own);
	void CheckLesson(entt::entity own);

	InputTargetInterface& _input;
	bool _active {false};
	fight_autopilot::Settings _settings;
	fight_autopilot::LessonStage _stage {fight_autopilot::LessonStage::Hit};
	std::mt19937 _random;
	Phase _phase {Phase::Waiting};
	uint64_t _frame {0};
	/// Game milliseconds since the start, in the phase, and to wait before the next press
	double _elapsedMs {0.0};
	float _phaseMs {0.0f};
	float _waitMs {0.0f};
	uint32_t _phaseFrames {0};
	size_t _nextSequenceMove {0};
	fight_autopilot::Intent _intent;
	/// The candidate pixel tried now, among the ones for a step or a double click that the pick must not find a creature
	/// at
	size_t _candidate {0};
	std::optional<glm::ivec2> _lastAim;
	/// The double click into the arena: the step it is at, and the game time until the next try
	uint32_t _clickStep {0};
	float _viewRetryMs {0.0f};
	/// The fight in the transcript, and how far its records are read
	std::optional<uint32_t> _fight;
	size_t _readTo {0};
	bool _checkedLesson {false};
	/// The game time at which the lesson is checked, once the bout has started
	std::optional<double> _lessonCheckDueMs;
	fight_autopilot::Status _counts;
	std::vector<fight_autopilot::LogEntry> _log;
};

} // namespace openblack::inspector
