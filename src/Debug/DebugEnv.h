/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdlib>

#include <optional>
#include <string>

// The debug trace switches that several files read: each is on while its OPENBLACK_*_TRACE environment variable is
// set, whatever its value. Read on every call; a caller that keeps the answer keeps it itself.

namespace openblack::debug_env
{

/// An environment variable read once, when it is made: a hook that runs every turn keeps one in a static instead of
/// searching the environment each time (nothing changes an OPENBLACK_* variable while the game runs). It keeps a copy
/// of the value, because getenv's pointer may not outlive a later change of the environment
class Variable
{
public:
	explicit Variable(const char* name)
	{
		if (const char* value = std::getenv(name); value != nullptr)
		{
			_value = value;
		}
	}
	/// The value as getenv gave it, or nullptr when the variable was not set
	[[nodiscard]] const char* Get() const { return _value ? _value->c_str() : nullptr; }

private:
	std::optional<std::string> _value;
};

/// OPENBLACK_ANIM_TRACE: the traces of the animation sound and effect cues
[[nodiscard]] inline bool AnimTrace()
{
	return std::getenv("OPENBLACK_ANIM_TRACE") != nullptr;
}

/// OPENBLACK_ANIMAL_TRACE: the traces of the animals' AI, lairs and wall hugging
[[nodiscard]] inline bool AnimalTrace()
{
	return std::getenv("OPENBLACK_ANIMAL_TRACE") != nullptr;
}

/// OPENBLACK_ATMOS_TRACE: the traces of the atmosphere sound banks
[[nodiscard]] inline bool AtmosTrace()
{
	return std::getenv("OPENBLACK_ATMOS_TRACE") != nullptr;
}

/// OPENBLACK_AUDIO_TRACE: the traces of the audio engine and its banks
[[nodiscard]] inline bool AudioTrace()
{
	return std::getenv("OPENBLACK_AUDIO_TRACE") != nullptr;
}

/// OPENBLACK_GESTURE_TRACE: the traces of the gestures
[[nodiscard]] inline bool GestureTrace()
{
	return std::getenv("OPENBLACK_GESTURE_TRACE") != nullptr;
}

/// OPENBLACK_GUIDANCE_TRACE: the traces of the guidance voices
[[nodiscard]] inline bool GuidanceTrace()
{
	return std::getenv("OPENBLACK_GUIDANCE_TRACE") != nullptr;
}

/// OPENBLACK_HAND_TRACE: the traces of the hand
[[nodiscard]] inline bool HandTrace()
{
	return std::getenv("OPENBLACK_HAND_TRACE") != nullptr;
}

/// OPENBLACK_MUSIC_TRACE: the traces of the music
[[nodiscard]] inline bool MusicTrace()
{
	return std::getenv("OPENBLACK_MUSIC_TRACE") != nullptr;
}

/// OPENBLACK_PHYSICS_TRACE: the traces of the physics objects
[[nodiscard]] inline bool PhysicsTrace()
{
	return std::getenv("OPENBLACK_PHYSICS_TRACE") != nullptr;
}

/// OPENBLACK_PSYS_SOUND_TRACE: the traces of the particle effects' sounds
[[nodiscard]] inline bool PsysSoundTrace()
{
	return std::getenv("OPENBLACK_PSYS_SOUND_TRACE") != nullptr;
}

/// OPENBLACK_SHADOW_TRACE: the traces of the shadows
[[nodiscard]] inline bool ShadowTrace()
{
	return std::getenv("OPENBLACK_SHADOW_TRACE") != nullptr;
}

/// OPENBLACK_SPELL_TRACE: the traces of the spells
[[nodiscard]] inline bool SpellTrace()
{
	return std::getenv("OPENBLACK_SPELL_TRACE") != nullptr;
}

/// OPENBLACK_TELEPORT_TRACE: the traces of the teleport
[[nodiscard]] inline bool TeleportTrace()
{
	return std::getenv("OPENBLACK_TELEPORT_TRACE") != nullptr;
}

/// OPENBLACK_TEXT_TRACE: the traces of the help texts
[[nodiscard]] inline bool TextTrace()
{
	return std::getenv("OPENBLACK_TEXT_TRACE") != nullptr;
}

/// OPENBLACK_TOWN_TRACE: the traces of the towns
[[nodiscard]] inline bool TownTrace()
{
	return std::getenv("OPENBLACK_TOWN_TRACE") != nullptr;
}

/// OPENBLACK_WEATHER_TRACE: the traces of the weather
[[nodiscard]] inline bool WeatherTrace()
{
	return std::getenv("OPENBLACK_WEATHER_TRACE") != nullptr;
}

} // namespace openblack::debug_env
