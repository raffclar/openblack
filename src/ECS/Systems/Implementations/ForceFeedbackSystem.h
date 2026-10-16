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
#include <deque>
#include <memory>
#include <optional>

#include "ECS/Systems/ForceFeedbackSystemInterface.h"
#include "ForceFeedback/ForceFeedbackBackend.h"
#include "ForceFeedback/ForceFeedbackRules.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class ForceFeedbackSystem final: public ForceFeedbackSystemInterface
{
public:
	/// The device and the effect file that its kind takes. Without a device the system does nothing.
	void Connect(std::unique_ptr<force_feedback::Backend> backend, const ifr::IFRFile& file);
	void Disconnect();

	[[nodiscard]] bool DeviceExists() const override;
	[[nodiscard]] std::string_view DeviceName() const override;
	[[nodiscard]] int Level() const override;
	void SetLevel(int level) override;

	void Start(ImmersionEffectType type) override;
	void StartTimes(ImmersionEffectType type, uint32_t iterations) override;
	void Stop(ImmersionEffectType type) override;
	void StopAll() override;
	[[nodiscard]] bool IsPlaying(ImmersionEffectType type) const override;
	void QuickenBeat(ImmersionEffectType type, float fraction) override;
	void SetPeriodAndMagnitude(ImmersionEffectType type, uint32_t periodMs, uint32_t magnitude) override;

	void SetHandTexture(std::optional<ImmersionEffectType> type) override;
	void TouchCreature(bool touchesBody) override;

	void Update(float seconds, glm::vec2 cursorShare) override;
	[[nodiscard]] std::vector<ForceFeedbackEvent> Events() const override;
	[[nodiscard]] float Strength() const override;

private:
	struct Slot
	{
		/// The effect as the file has it, with the changes that the game made to it since
		force_feedback::Effect effect;
		/// The device's clock when the effect started, none when it does not play
		std::optional<double> startedAt;
		std::optional<uint32_t> iterations;
	};

	[[nodiscard]] Slot* SlotFor(ImmersionEffectType type);
	[[nodiscard]] const Slot* SlotFor(ImmersionEffectType type) const;
	void Begin(ImmersionEffectType type, std::optional<uint32_t> iterations);
	void Record(ImmersionEffectType type, bool start, std::optional<uint32_t> iterations);

	std::unique_ptr<force_feedback::Backend> _backend;
	std::array<std::optional<Slot>, force_feedback::k_Effects.size()> _slots;
	int _level {force_feedback::k_DefaultLevel};
	double _clock {0.0};
	force_feedback::HandTextureState _handTexture;
	std::optional<glm::vec2> _lastCursor;
	std::deque<ForceFeedbackEvent> _events;
	float _strength {0.0f};
};

} // namespace openblack::ecs::systems
