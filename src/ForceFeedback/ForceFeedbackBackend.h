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
#include <string>
#include <string_view>

#include "ForceFeedbackRules.h"

namespace openblack::force_feedback
{

/// A device that shows the force-feedback effects
class Backend
{
public:
	virtual ~Backend() = default;

	[[nodiscard]] virtual std::string_view Name() const = 0;
	/// The kind of device, which picks the effect file
	[[nodiscard]] virtual DeviceKind Kind() const = 0;
	/// The device shows this vibration, from 0 to 1, until the next call
	virtual void Vibrate(float strength) = 0;
	/// An effect starts or stops on the device
	virtual void Started(ImmersionEffectType type, std::string_view name) = 0;
	virtual void Stopped(ImmersionEffectType type, std::string_view name) = 0;
};

/// A device that writes each effect to the game's log and shows nothing, so that tests and agents can see the effects
/// without a device. It takes the effects of a device that only vibrates.
[[nodiscard]] std::unique_ptr<Backend> MakeLogBackend();

/// The first device of SDL's haptic interface that can vibrate (a gamepad or a joystick), none when there is none
[[nodiscard]] std::unique_ptr<Backend> MakeSdlHapticBackend();

} // namespace openblack::force_feedback
