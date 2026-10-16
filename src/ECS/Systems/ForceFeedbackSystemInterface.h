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
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// A start or a stop of a force-feedback effect that went to the device
struct ForceFeedbackEvent
{
	/// The time of the device's clock, in seconds
	double seconds {0.0};
	ImmersionEffectType type {ImmersionEffectType::None};
	bool start {false};
	/// The number of times the effect plays, none for the number of the effect file
	std::optional<uint32_t> iterations;
};

/// The force-feedback effects that the game sends to a device: a force-feedback mouse in the game, a device of SDL's
/// haptic interface or a log here. Without a device every call does nothing and the device does not exist. Only the
/// local player's events come here.
class ForceFeedbackSystemInterface
{
public:
	virtual ~ForceFeedbackSystemInterface() = default;

	/// True when a device takes the effects
	[[nodiscard]] virtual bool DeviceExists() const = 0;
	/// The name of the device, empty without a device
	[[nodiscard]] virtual std::string_view DeviceName() const = 0;
	/// The detail level of the player's profile, from 0 to 2: an effect plays only when its level is at most this level
	[[nodiscard]] virtual int Level() const = 0;
	/// A new detail level stops all the effects
	virtual void SetLevel(int level) = 0;

	/// The effect starts from its beginning, as many times as the effect file says
	virtual void Start(ImmersionEffectType type) = 0;
	/// The effect starts from its beginning and plays the given number of times
	virtual void StartTimes(ImmersionEffectType type, uint32_t iterations) = 0;
	virtual void Stop(ImmersionEffectType type) = 0;
	virtual void StopAll() = 0;
	[[nodiscard]] virtual bool IsPlaying(ImmersionEffectType type) const = 0;
	/// The beat of the effect becomes quicker by a fraction of its period from the file, and keeps its number of beats
	virtual void QuickenBeat(ImmersionEffectType type, float fraction) = 0;
	/// The effect gets a new period in milliseconds and a new strength from 0 to 10000
	virtual void SetPeriodAndMagnitude(ImmersionEffectType type, uint32_t periodMs, uint32_t magnitude) = 0;

	/// The effect of what the hand holds or touches in this frame, none for no effect
	virtual void SetHandTexture(std::optional<ImmersionEffectType> type) = 0;
	/// The hand on a creature touches its body in this frame, or does not
	virtual void TouchCreature(bool touchesBody) = 0;

	/// The device plays on for the real time of a frame, with the cursor as a share of the screen from 0 to 1
	virtual void Update(float seconds, glm::vec2 cursorShare) = 0;
	/// The newest starts and stops, oldest first
	[[nodiscard]] virtual std::vector<ForceFeedbackEvent> Events() const = 0;
	/// The vibration that the device shows now, from 0 to 1
	[[nodiscard]] virtual float Strength() const = 0;
};

} // namespace openblack::ecs::systems
