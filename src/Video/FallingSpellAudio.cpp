/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FallingSpellAudio.h"

#include <string>

#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/vec3.hpp>

#include "Audio/AudioManagerInterface.h"
#include "Game.h"
#include "Gui/GameInterface.h"
#include "Locator.h"

using namespace openblack::video;

namespace
{
constexpr std::string_view k_InGame = "InGame.sad";
constexpr std::string_view k_Spells = "spells.sad";
constexpr std::string_view k_ScriptSfx = "Scriptsfx.sad";
} // namespace

std::optional<FallingSpellSound> openblack::video::SoundOf(FallingSpellCue cue) noexcept
{
	using enum FallingSpellCue;
	using Action = FallingSpellSound::Action;
	switch (cue)
	{
	case Rumble:
		return FallingSpellSound {.bank = k_ScriptSfx, .sample = 151};
	case LaserExplode:
		return FallingSpellSound {.bank = k_Spells, .sample = 56};
	case VolcanoStop:
		return FallingSpellSound {.action = Action::Stop, .bank = k_InGame, .sample = 172, .owner = 1};
	case Creed:
		return FallingSpellSound {.bank = k_InGame, .sample = 166};
	case CitadelExplode:
	case CitadelExplodeEnd:
		return FallingSpellSound {.bank = k_InGame, .sample = 168};
	case Volcano:
		return FallingSpellSound {.bank = k_InGame, .sample = 172, .owner = 1};
	case HealChakra:
		return FallingSpellSound {.bank = k_Spells, .sample = 30};
	case CreedHigh:
		return FallingSpellSound {.bank = k_InGame, .sample = 166, .owner = 2, .pitchPercent = 133};
	case CreedStop:
		return FallingSpellSound {.action = Action::Stop, .bank = k_InGame, .sample = 166};
	case CreedHighStop:
		return FallingSpellSound {.action = Action::Stop, .bank = k_InGame, .sample = 166, .owner = 2};
	case MusicFadeOut:
	case WhiteFadeStart:
	case WhiteFadeBack:
		break;
	}
	return std::nullopt;
}

void FallingSpellAudio::Play(FallingSpellCue cue)
{
	if (!Locator::audio::has_value())
	{
		return;
	}
	auto& audio = Locator::audio::value();
	switch (cue)
	{
	case FallingSpellCue::MusicFadeOut:
		audio.MusicStop(true);
		return;
	case FallingSpellCue::WhiteFadeStart:
		if (auto* game = Game::Instance(); game != nullptr && game->GetInterface() != nullptr)
		{
			game->GetInterface()->GetScreenFade().FadeThrough(glm::vec3(1.0f));
		}
		return;
	case FallingSpellCue::WhiteFadeBack:
		// The temple's fade comes back from white by itself once it gets there
		return;
	default:
		break;
	}
	const auto sound = SoundOf(cue);
	if (!sound)
	{
		return;
	}
	const auto key = std::pair(sound->sample, sound->owner);
	if (sound->action == FallingSpellSound::Action::Stop)
	{
		if (const auto it = _playing.find(key); it != _playing.end())
		{
			if (audio.EmitterExists(it->second))
			{
				audio.StopEmitter(it->second);
			}
			_playing.erase(it);
		}
		return;
	}
	// A sound played again with the same sample and owner restarts rather than playing twice
	if (const auto it = _playing.find(key); it != _playing.end() && audio.EmitterExists(it->second))
	{
		audio.StopEmitter(it->second);
	}
	const auto name = fmt::format("{}/{}", sound->bank, sound->sample);
	_playing[key] = audio.StartSoundEffect(entt::hashed_string(name.c_str()).value(), {.pitchPercent = sound->pitchPercent});
}
