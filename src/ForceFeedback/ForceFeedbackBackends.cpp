/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <string>

#include <SDL.h>
#include <spdlog/spdlog.h>

#include "ForceFeedbackBackend.h"

namespace openblack::force_feedback
{

namespace
{

/// The smallest change of strength that the device gets, so that it does not get a new value each frame for nothing
constexpr float k_StrengthStep = 0.01f;
/// The time in milliseconds that a vibration lasts when the next frame does not come
constexpr uint32_t k_VibrationMs = 250;

void Log(std::string_view text)
{
	if (auto logger = spdlog::get("game"))
	{
		SPDLOG_LOGGER_INFO(logger, "{}", text);
	}
}

class LogBackend final: public Backend
{
public:
	[[nodiscard]] std::string_view Name() const override { return "log"; }
	[[nodiscard]] DeviceKind Kind() const override { return DeviceKind::Tactile; }
	void Vibrate(float /*strength*/) override {}
	void Started(ImmersionEffectType type, std::string_view name) override
	{
		Log(std::string("Force feedback starts ") + std::string(name) + " (" + std::to_string(static_cast<int>(type)) + ")");
	}
	void Stopped(ImmersionEffectType type, std::string_view name) override
	{
		Log(std::string("Force feedback stops ") + std::string(name) + " (" + std::to_string(static_cast<int>(type)) + ")");
	}
};

class SdlHapticBackend final: public Backend
{
public:
	SdlHapticBackend(SDL_Haptic* haptic, std::string name)
	    : _haptic(haptic)
	    , _name(std::move(name))
	{
	}
	SdlHapticBackend(const SdlHapticBackend&) = delete;
	SdlHapticBackend& operator=(const SdlHapticBackend&) = delete;
	SdlHapticBackend(SdlHapticBackend&&) = delete;
	SdlHapticBackend& operator=(SdlHapticBackend&&) = delete;
	~SdlHapticBackend() override
	{
		SDL_HapticRumbleStop(_haptic);
		SDL_HapticClose(_haptic);
		SDL_QuitSubSystem(SDL_INIT_HAPTIC | SDL_INIT_JOYSTICK);
	}

	[[nodiscard]] std::string_view Name() const override { return _name; }
	[[nodiscard]] DeviceKind Kind() const override { return DeviceKind::Tactile; }
	void Vibrate(float strength) override
	{
		if (std::abs(strength - _shown) < k_StrengthStep && strength > 0.0f)
		{
			return;
		}
		if (strength <= 0.0f)
		{
			if (_shown > 0.0f)
			{
				SDL_HapticRumbleStop(_haptic);
			}
		}
		else
		{
			SDL_HapticRumblePlay(_haptic, strength, k_VibrationMs);
		}
		_shown = strength;
	}
	void Started(ImmersionEffectType /*type*/, std::string_view /*name*/) override {}
	void Stopped(ImmersionEffectType /*type*/, std::string_view /*name*/) override {}

private:
	SDL_Haptic* _haptic;
	std::string _name;
	float _shown {0.0f};
};

} // namespace

std::unique_ptr<Backend> MakeLogBackend()
{
	return std::make_unique<LogBackend>();
}

std::unique_ptr<Backend> MakeSdlHapticBackend()
{
	if (SDL_InitSubSystem(SDL_INIT_HAPTIC | SDL_INIT_JOYSTICK) != 0)
	{
		return nullptr;
	}
	for (int i = 0; i < SDL_NumHaptics(); ++i)
	{
		SDL_Haptic* haptic = SDL_HapticOpen(i);
		if (haptic == nullptr)
		{
			continue;
		}
		if (SDL_HapticRumbleSupported(haptic) == SDL_TRUE && SDL_HapticRumbleInit(haptic) == 0)
		{
			const char* name = SDL_HapticName(i);
			return std::make_unique<SdlHapticBackend>(haptic, name != nullptr ? name : "haptic device");
		}
		SDL_HapticClose(haptic);
	}
	SDL_QuitSubSystem(SDL_INIT_HAPTIC | SDL_INIT_JOYSTICK);
	return nullptr;
}

} // namespace openblack::force_feedback
