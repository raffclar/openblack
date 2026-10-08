/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FallingSpellVideo.h"

#include <exception>
#include <string_view>
#include <utility>

#include "3D/ScreenFade.h"
#include "Audio/Audio.h"
#include "ECS/Components/Creature.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ScreenFadeSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "Enums.h"
#include "FileSystem/FileSystemInterface.h"
#include "Game.h"
#include "GameClock.h"
#include "Locator.h"
#include "VideoPlayer.h"

using namespace openblack;
using namespace openblack::video;

namespace
{
/// The sequence mode that runs neither fade
constexpr int32_t k_SequenceModeNoFade = 3;
/// The frame delta is in milliseconds, the fade speed in seconds
constexpr float k_MsToSeconds = 0.001f;
/// The fade value (0..1) to an alpha byte
constexpr float k_AlphaScale = 255.0f;
/// The falling spell's film, relative to the game folder
constexpr std::string_view k_FilmPath = "Data/Spells/fall/fall.bik";
} // namespace

int32_t video::FallingSpellFilmMs(int32_t frame, int32_t fps) noexcept
{
	if (fps == 0)
	{
		return 0; // (openblack) the callers never divide by 0: fps <= 0 becomes 24 first
	}
	// frame * 1000 in 32 bits (wrapping), then a signed division
	const auto product = static_cast<int32_t>(static_cast<uint32_t>(frame) * 1000u);
	return product / fps;
}

bool TempleFade::Runs(int32_t mode) const noexcept
{
	if (mode == k_SequenceModeNoFade)
	{
		return false;
	}
	// Runs while the fade has not reached its target, and always in the citadel's mode; otherwise the script fade runs
	return target != current || mode == k_SequenceModeCitadel;
}

uint32_t TempleFade::Update(uint32_t deltaMs) noexcept
{
	// The original runs its floating point at single precision, so every step here rounds to float, as float
	// arithmetic does. (inferred) that nothing raises the precision again between frames
	const float step = static_cast<float>(deltaMs) * k_MsToSeconds;
	uint32_t colourRgb = rgb;
	if (current < target)
	{
		const float value = current + step;
		current = value;
		// Only past the target: landing on it exactly leaves target == current, the fade then stops running and is
		// held, and `done` never goes up
		if (value > target)
		{
			++done;
			current = target;
			target = 0.0f;
		}
	}
	else
	{
		const float value = current - step;
		current = value;
		if (value < target)
		{
			current = target;
			++done;
			if (target <= 0.0f)
			{
				rgb = 0;
				colourRgb = 0;
			}
		}
	}
	// 0xFF above 1.0, else current * 255 rounded to float and truncated
	const uint32_t alpha = current > 1.0f ? 0xFFu : static_cast<uint32_t>(static_cast<int32_t>(current * k_AlphaScale));
	// alpha in the top byte over the colour's RGB, for the screen fade
	return (alpha << 24) + (colourRgb & 0x00FFFFFFu);
}

FallingSpellVideo::Hooks FallingSpellVideo::GameHooks()
{
	Hooks hooks;
	// (inferred) the local player is PLAYER_ONE (as audio's localPlayerNumber, Game.cpp), and its creature any entity
	// with a Creature of that owner
	hooks.hasCreature = []() {
		if (!Locator::entitiesRegistry::has_value())
		{
			return false;
		}
		bool found = false;
		Locator::entitiesRegistry::value().Each<const ecs::components::Creature>(
		    [&found](entt::entity, const ecs::components::Creature& creature) {
			    found = found || creature.owner == PlayerNames::PLAYER_ONE;
		    });
		return found;
	};
	hooks.filmPath = []() {
		const std::filesystem::path path = k_FilmPath;
		if (!Locator::filesystem::has_value())
		{
			return path;
		}
		try
		{
			return Locator::filesystem::value().FindPath(path);
		}
		catch (const std::exception&)
		{
			return path; // the player is made anyway and ends on the next frame
		}
	};
	// The update's 11 sounds, all 2D: a Play uses the default play options with the bank, owner and sample (and a
	// pitch of 133 once); a Stop stops that bank, owner and sample. The owners 1 and 2 are not things: Owner::Key, so
	// each loop can be stopped alone
	hooks.sound = [](const FallingSpellSound& sound) {
		const auto bank = static_cast<audio::SfxBank>(sound.bank); // 1 InGame, 3 Spells, 5 ScriptSfx (BankTables.h)
		const auto owner = sound.owner == 0 ? audio::Owner::None() : audio::Owner::Key(sound.owner);
		if (sound.kind == FallingSpellSound::Kind::Stop)
		{
			audio::StopSoundEffect(sound.sample, owner, bank);
			return;
		}
		audio::PlayOptions options;
		options.sample = {audio::Bank(bank), sound.sample};
		options.owner = owner;
		options.is3D = false;
		options.pitch = sound.pitch; // the default 100 but for one sound
		audio::PlaySoundEffect(options);
	};
	// Every music channel fades out
	hooks.musicStop = [](int32_t fade) { audio::MusicStop(fade); };
	hooks.setScreenFadeColour = [](uint32_t argb) {
		if (Locator::screenFade::has_value())
		{
			Locator::screenFade::value().Fade().SetColour(argb);
		}
	};
	return hooks;
}

FallingSpellVideo::FallingSpellVideo(VideoPlayer& player, Hooks hooks)
    : _player(player)
    , _hooks(std::move(hooks))
{
}

void FallingSpellVideo::KickOff()
{
	if (!_hooks.hasCreature || !_hooks.hasCreature())
	{
		return; // no creature, nothing at all
	}
	End(); // the previous one, if any
	Start();
}

void FallingSpellVideo::Start()
{
	game_clock::SetSequenceMode(k_SequenceModeFallingSpell);
	// A failed allocation of the original's object is not modelled
	_active = true;
	_player.SetFallingSpellVideo(true);
	Init();
}

void FallingSpellVideo::Init()
{
	// (not ported) the camera path, the falling creature and its hand glows
	_lastMs = 0x64; // 100
	const std::filesystem::path path = _hooks.filmPath ? _hooks.filmPath() : std::filesystem::path(k_FilmPath);
	_player.Play(path);
	if (_player.IsPlaying())
	{
		// (approximate) the fallback fps of 24 is applied in Update instead; a film that opens always has fps >= 1
		// here, BikFile refuses 0
		// (not ported) the camera at the path's point of frame * 1000 / fps, its points * 0.8
		_player.SetSchedule(_player.EndFrame(), _player.EndFrame()); // the film has no fade of its own
	}
	// (not ported) the camera position kept, the sprite and the 16 sprite records
	_sparklesOn = false;
	_state = 0;
	_soundState = 0;
	// (not ported) the light bursts and the finish frame callback
}

void FallingSpellVideo::Close()
{
	// (not ported) the finish frame callback removed, the creature deleted, the path freed, the light put back, the
	// sprites
}

void FallingSpellVideo::End()
{
	if (!_active)
	{
		return;
	}
	game_clock::SetSequenceMode(k_SequenceModeNone);
	Close();
	_active = false;
	_player.SetFallingSpellVideo(false);
	_player.Skip(); // without the falling spell: the 48-frame fade (or the end in the fade zone)
}

void FallingSpellVideo::Sound(FallingSpellSound::Kind kind, FallingSpellSound::Bank bank, int32_t sample, uint32_t owner,
                              FallingSpellSound::Cue cue, int32_t pitch) const
{
	if (_hooks.sound)
	{
		_hooks.sound(FallingSpellSound {kind, bank, sample, owner, pitch, cue});
	}
}

void FallingSpellVideo::Update()
{
	using Kind = FallingSpellSound::Kind;
	using Bank = FallingSpellSound::Bank;
	using Cue = FallingSpellSound::Cue;
	if (!_active)
	{
		return;
	}
	if (!_player.IsPlaying())
	{
		++_state;
		return;
	}
	int32_t fps = _player.Fps();
	if (fps <= 0)
	{
		fps = k_FallingSpellFallbackFps;
	}
	const int32_t ms = FallingSpellFilmMs(_player.CurrentFrame(), fps);
	// (not ported) the creature animated by ms - _lastMs
	_lastMs = ms;
	// (not ported) the camera on the path at ms, with a pi / 4 field of view

	// The sound state, each test after the one before (one call can go through all three)
	if (_soundState == 0 && ms > k_FallingSpellSoundOneMs)
	{
		_soundState = 1;
		Sound(Kind::Play, Bank::ScriptSfx, 0x97, 0, Cue::Rumble); // 151 ScreenRumble
	}
	if (_soundState == 1 && ms > k_FallingSpellSoundTwoMs)
	{
		_soundState = 2;
		Sound(Kind::Play, Bank::Spells, 0x38, 0, Cue::LaserExplode); // 56 S_LasersbeamExplode_02
		Sound(Kind::Stop, Bank::InGame, 0xAC, 1, Cue::VolcanoStop);  // 172 G_Volcano_02, owner 1
	}
	if (_soundState == 2 && ms > k_FallingSpellSoundThreeMs)
	{
		_soundState = 3;
		Sound(Kind::Play, Bank::InGame, 0xA6, 0, Cue::Creed); // 166 G_Creed_01
	}

	// The state
	if (_state == 0 && ms > k_FallingSpellStateOneMs)
	{
		_sparklesOn = true;
		_state = 1;
		Sound(Kind::Play, Bank::InGame, 0xA8, 0, Cue::CitadelExplode); // 168 G_CitadelExplode_01
		Sound(Kind::Play, Bank::InGame, 0xAC, 1, Cue::Volcano);        // 172 G_Volcano_02, owner 1
	}
	if (_state == 1 && ms > k_FallingSpellStateTwoMs)
	{
		_state = 2;
		Sound(Kind::Play, Bank::Spells, 0x1E, 0, Cue::HealChakra);      // 30 S_HealChakra
		Sound(Kind::Play, Bank::InGame, 0xA6, 2, Cue::CreedHigh, 0x85); // 166 G_Creed_01, owner 2, pitch 133
	}
	if (_state == 2 && ms > k_FallingSpellStateThreeMs)
	{
		// The Temple fade to white, from 0 to 1
		_fade.rgb = k_FallingSpellFadeRgb;
		_fade.target = 1.0f;
		_fade.current = 0.0f;
		_fade.done = 0;
		Sound(Kind::Stop, Bank::InGame, 0xA6, 0, Cue::CreedStop);     // 166 G_Creed_01
		Sound(Kind::Stop, Bank::InGame, 0xA6, 2, Cue::CreedHighStop); // 166 G_Creed_01, owner 2
		++_state;                                                     // -> 3
		if (_hooks.musicStop)
		{
			_hooks.musicStop(k_FallingSpellMusicFade);
		}
		Sound(Kind::Play, Bank::InGame, 0xA8, 0, Cue::CitadelExplodeEnd); // 168 G_CitadelExplode_01
	}
	if (_state == 3 && _fade.done != 0)
	{
		_state = k_FallingSpellEndState;
		// The fade back from white, from 1 to 0
		_fade.rgb = k_FallingSpellFadeRgb;
		_fade.target = 0.0f;
		_fade.current = 1.0f;
		_fade.done = 0;
	}
}

void FallingSpellVideo::ProcessFrame(uint32_t realMs)
{
	if (Mode() == k_SequenceModeFallingSpell)
	{
		// (not ported) the atmosphere update
		Update();
		// (openblack) `!_active`: the original reads the state with no test (the falling spell mode without the object
		// only after a failed allocation, which crashes in Init)
		if (!_player.IsPlaying() || !_active || _state == k_FallingSpellEndState)
		{
			End();
		}
		// else the renderer draws the film; the creature, the sprites and the liquid particles are not ported
	}
	if (_fade.Runs(Mode()))
	{
		const uint32_t colour = _fade.Update(realMs);
		if (_hooks.setScreenFadeColour)
		{
			_hooks.setScreenFadeColour(colour);
		}
	}
	// else the script fade: openblack's ScreenFade::ProcessTurn, once a turn (Game.cpp)
}

FallingSpellVideo& video::GetFallingSpell()
{
	return Locator::videoSystem::value().FallingSpell();
}
