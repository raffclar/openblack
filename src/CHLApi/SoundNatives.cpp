/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cctype>
#include <cmath>
#include <cstdint>
#include <ctime>

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include <LHVM.h>
#include <LHVMTypes.h>
#include <SaveSlots.h>
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/matrix.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "CHLApi.h"
#include "Sound/GameMusic.h"
#include "Sound/GameSoundEffects.h"
#include "Sound/ScriptSoundEffect.h"
#include "Sound/SoundRequestInterface.h"
#if defined(OPENBLACK_DEVELOPER_TOOLS)
#include "Common/DrawTrace.h"
#endif
#include "CHLApi/NativeHelpers.h"
#include "CHLApi/Natives.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/BoatLaunchSystemInterface.h"
#include "ECS/Systems/CreatureAudioSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/EffectRequestSystemInterface.h"
#include "ECS/Systems/HandDemoSystemInterface.h"
#include "ECS/Systems/HelpSpeechSystemInterface.h"
#include "ECS/Systems/HelpTextSystemInterface.h"
#include "ECS/Systems/ParticleRuleSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/SoundTagSystemInterface.h"
#include "ECS/Systems/TempleDestructionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TurnCheerSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/WalkPathSystemInterface.h"
#include "ECS/WalkerPlacement.h"
#include "Hand/HandGrabRules.h"
#include "Help/DialogueText.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::chlapi
{

using openblack::Locator;

/// The loaded sound group of a bank's file, whatever the case of the file's name on disk
static std::optional<std::string> LoadedSoundGroup(std::string_view file)
{
	const auto sameName = [file](const std::string& name) {
		return std::ranges::equal(name, file, [](char a, char b) {
			return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
		});
	};
	for (const auto& name : Locator::sounds::value().GetSoundGroups() | std::views::keys)
	{
		if (sameName(name))
		{
			return name;
		}
	}
	return std::nullopt;
}

/// The sound of a script's sample in a script sound bank; none for a number naming no bank, a bank the game doesn't
/// ship or a sample the bank doesn't have
static std::optional<entt::id_type> BankSound(int32_t bank, int32_t sample)
{
	const auto file = audio::ScriptSoundBankFile(bank);
	if (!file.has_value() || !Locator::sounds::has_value() || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto group = LoadedSoundGroup(*file);
	if (!group.has_value())
	{
		return std::nullopt;
	}
	const auto id = entt::hashed_string(fmt::format("{}/{}", *group, sample).c_str()).value();
	if (!Locator::resources::value().GetSounds().Contains(id))
	{
		return std::nullopt;
	}
	return id;
}

void CHLApi::PlayBankSoundEffect(int32_t bank, int32_t sample, const audio::SoundEffectOptions& options) const
{
	// A number naming no bank, a bank the game doesn't ship and a sample the bank doesn't have all play nothing
	const auto found = BankSound(bank, sample);
	if (!found.has_value())
	{
		return;
	}
	const auto id = *found;
	auto& sounds = Locator::resources::value().GetSounds();

	const auto conditions = audio::CurrentSoundEffectConditions();
	if (!audio::SoundEffectHeard(conditions, static_cast<audio::ScriptSoundBank>(bank), sounds.Handle(id)->userParam))
	{
		return;
	}
	Locator::sounds::value().StartSoundEffect(id, options);
}

void PlaySoundEffect() // 043 PLAY_SOUND_EFFECT
{
	const auto withPosition = Pop().intVal != 0;
	const auto position = PopVec();
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	// The sample number is the owner of a script's sound
	Locator::chlapi::value().PlayBankSoundEffect(bank, sample,
	                                             {.position = withPosition ? std::optional(position) : std::nullopt,
	                                              .voiceOwner = audio::VoiceOwner::Number,
	                                              .ownerNumber = static_cast<uint32_t>(sample)});
}

/// The game's music, none before the game has started it
static audio::GameMusic* ScriptGameMusic()
{
	return Locator::gameMusic::has_value() ? &Locator::gameMusic::value() : nullptr;
}

/// The music that the scripts start, as the world sees it
ecs::systems::ScriptMusicSystemInterface* WorldScriptMusic()
{
	return Locator::scriptMusicSystem::has_value() ? &Locator::scriptMusicSystem::value() : nullptr;
}

/// Without the music the scripts that wait on music wait on the help text instead
static bool WorldHasMusic()
{
	const auto* music = WorldScriptMusic();
	return music != nullptr && music->HasMusic();
}

static bool HelpTextRead()
{
	return !Locator::helpTextSystem::has_value() || Locator::helpTextSystem::value().IsTextRead();
}

void StartMusic() // 044 START_MUSIC
{
	const auto music = Pop().intVal;
	if (music < 0 || music >= static_cast<int32_t>(audio::MusicType::_COUNT))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "START_MUSIC: no music type {}", music);
		return;
	}
	if (auto* scriptMusic = WorldScriptMusic())
	{
		// The sung lines start again with each piece a script starts
		scriptMusic->ResetLyrics();
		scriptMusic->Start(static_cast<audio::MusicType>(music));
	}
}

void StopMusic() // 045 STOP_MUSIC
{
	if (auto* scriptMusic = WorldScriptMusic())
	{
		scriptMusic->Start(audio::MusicType::None);
	}
}

/// The object a music native is given, if it is still in the world
std::optional<uint32_t> PopMusicObject(std::string_view native)
{
	const auto object = PopObject();
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "{}: thing not valid", native);
		return std::nullopt;
	}
	return entt::to_integral(object);
}

void AttachMusic() // 046 ATTACH_MUSIC
{
	const auto object = PopMusicObject("ATTACH_MUSIC");
	const auto music = Pop().intVal;
	if (music < 1 || music >= static_cast<int32_t>(audio::MusicType::_COUNT))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "ATTACH_MUSIC: no music type {}", music);
	}
	auto* gameMusic = ScriptGameMusic();
	if (object && gameMusic != nullptr)
	{
		gameMusic->AttachMusic(*object, static_cast<audio::MusicType>(music));
	}
}

void DetachMusic() // 047 DETACH_MUSIC
{
	const auto object = PopMusicObject("DETACH_MUSIC");
	if (auto* gameMusic = ScriptGameMusic(); object && gameMusic != nullptr)
	{
		gameMusic->DetachMusic(*object);
	}
}

void MoveMusic() // 149 MOVE_MUSIC
{
	const auto to = PopMusicObject("MOVE_MUSIC");
	const auto from = PopMusicObject("MOVE_MUSIC");
	if (auto* gameMusic = ScriptGameMusic(); from && to && gameMusic != nullptr)
	{
		gameMusic->MoveMusic(*from, *to);
	}
}

void EnableDisableMusic() // 181 ENABLE_DISABLE_MUSIC
{
	const auto object = PopMusicObject("ENABLE_DISABLE_MUSIC");
	const auto enable = Pop().intVal != 0;
	// Music that is off does not play, but near the camera it keeps the music of the land from a start
	if (auto* gameMusic = ScriptGameMusic(); object && gameMusic != nullptr)
	{
		gameMusic->SetAttachedMusicActive(*object, enable);
	}
}

/// How far the music of a type carries, as the world reads it, 100 without the music
static float WorldMusicPlayDistance(audio::MusicType type)
{
	const auto* music = WorldScriptMusic();
	return music != nullptr ? music->PlayDistance(type) : audio::MusicPlayDistance(nullptr);
}

void GetMusicObjDistance() // 182 GET_MUSIC_OBJ_DISTANCE
{
	// How far the music attached to the object carries. An object with no attached music gives 0.
	const auto object = PopMusicObject("GET_MUSIC_OBJ_DISTANCE");
	const auto* gameMusic = ScriptGameMusic();
	if (!object || gameMusic == nullptr)
	{
		Pushf(0.0f);
		return;
	}
	const auto& attached = gameMusic->GetAttachedMusic();
	const auto found = std::ranges::find(attached, *object, &audio::GameMusic::AttachedMusic::thing);
	Pushf(found != attached.end() ? WorldMusicPlayDistance(found->type) : 0.0f);
}

void GetMusicEnumDistance() // 183 GET_MUSIC_ENUM_DISTANCE
{
	// How far the music of a type carries. For a type that is not a music type, the game pushes 0 and then also pushes
	// the distance of no bank, so the script gets two values (see docs/vanilla-bugs.md).
	const auto music = Pop().intVal;
	if (music < 1 || music >= static_cast<int32_t>(audio::MusicType::_COUNT))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_MUSIC_ENUM_DISTANCE: no music type {}", music);
		Pushf(0.0f);
	}
	Pushf(WorldMusicPlayDistance(static_cast<audio::MusicType>(music)));
}

void SetMusicPlayPosition() // 184 SET_MUSIC_PLAY_POSITION
{
	const auto position = PopVec();
	const auto object = PopMusicObject("SET_MUSIC_PLAY_POSITION");
	// The music of the object sounds from the place. The game keeps the place as a map position with its height above
	// the ground there.
	if (auto* gameMusic = ScriptGameMusic(); object && gameMusic != nullptr && Locator::terrainSystem::has_value())
	{
		gameMusic->SetAttachedMusicPlace(*object, map_coords::FromWorld(Locator::terrainSystem::value(), position));
	}
}

void RestartMusic() // 190 RESTART_MUSIC
{
	const auto object = PopMusicObject("RESTART_MUSIC");
	if (auto* gameMusic = ScriptGameMusic(); object && gameMusic != nullptr)
	{
		gameMusic->RestartAttachedMusic(*object);
	}
}

void MusicPlayed191() // 191 MUSIC_PLAYED
{
	// Whether the music attached to an object has finished; an object that has gone counts as finished
	const auto object = PopMusicObject("MUSIC_PLAYED");
	const auto* gameMusic = ScriptGameMusic();
	Pushb(!object || gameMusic == nullptr || gameMusic->IsAttachedMusicFinished(*object));
}

/// The advisors start cheering the player's turning of the camera, the measure of its speed starting again from
/// still, or stop
void StartAngleSound(bool enable)
{
	// Only a computer that shows the world cheers its player on
	if (!Locator::turnCheerSystem::has_value())
	{
		return;
	}
	if (enable)
	{
		Locator::turnCheerSystem::value().Start();
	}
	else
	{
		Locator::turnCheerSystem::value().Stop();
	}
}

void StartAngleSound285() // 285 START_ANGLE_SOUND
{
	StartAngleSound(Pop().uintVal != 0);
}

void SetCreatureSound() // 317 SET_CREATURE_SOUND
{
	// Off, the creatures of players other than this computer's are not heard in their own voices
	const auto enable = Pop().intVal != 0;
	if (Locator::creatureAudioSystem::has_value())
	{
		Locator::creatureAudioSystem::value().SetOtherVoicesEnabled(enable);
	}
}

void LastMusicLine() // 335 LAST_MUSIC_LINE
{
	// Whether the script's sung music has reached a line: the sung verses wait for each line before showing it
	const auto line = static_cast<int32_t>(Popf());
	if (!WorldHasMusic())
	{
		Pushb(HelpTextRead());
		return;
	}
	Pushb(audio::ReachedMusicLine(WorldScriptMusic()->GetMusic().GetLyrics().line, line));
}

void GamePlaySaySoundEffect() // 340 GAME_PLAY_SAY_SOUND_EFFECT
{
	// Says a help text's line, on the second voice for "extra", heard from the position when it has one
	const auto withPosition = Pop().intVal != 0;
	const auto position = PopVec();
	const auto text = static_cast<uint32_t>(Pop().intVal);
	const auto extra = Pop().intVal != 0;
	Locator::helpSpeechSystem::value().Say(text, extra ? audio::SpeechVoice::Second : audio::SpeechVoice::First,
	                                       withPosition ? std::optional(position) : std::nullopt);
}

void StartAngleSound348() // 348 START_ANGLE_SOUND
{
	StartAngleSound(Pop().uintVal != 0);
}

void MusicPlayed350() // 350 MUSIC_PLAYED
{
	// Whether the script music of a type is over
	const auto music = Pop().intVal;
	if (!WorldHasMusic())
	{
		Pushb(HelpTextRead());
		return;
	}
	Pushb(WorldScriptMusic()->GetMusic().IsOver(static_cast<audio::MusicType>(music)));
}

void SetGameSound() // 357 SET_GAME_SOUND
{
	// Turned off, every sound effect playing stops and from then only the advisors' and villagers' speech plays
	const auto enable = Pop().intVal != 0;
	if (!enable)
	{
		Locator::sounds::value().StopAllSoundEffects();
	}
	Locator::chlapi::value().SetGameSoundOn(enable);
}

void SoundExists() // 409 SOUND_EXISTS
{
	// Whether the game has a sound device to play through
	Pushb(Locator::sounds::has_value() && Locator::sounds::value().IsInstalled());
}

void StopSoundEffect() // 424 STOP_SOUND_EFFECT
{
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	const auto speech = Pop().intVal != 0;
	if (speech)
	{
		// The sample is then a help text whose spoken line is stopped. No script passes it.
		// TODO(raffclar): stop a help text's speech
		NotImplemented();
		return;
	}
	// A script's sound has its sample number as its owner, so the stop leaves the copies that the game plays
	if (const auto sound = BankSound(bank, sample))
	{
		Locator::sounds::value().StopNumberedSoundEffect(*sound, static_cast<uint32_t>(sample));
	}
}

void EnableDisableAlignmentMusic() // 445 ENABLE_DISABLE_ALIGNMENT_MUSIC
{
	const auto enable = Pop().intVal != 0;
	SPDLOG_LOGGER_DEBUG(spdlog::get("scripting"), "The land's music is turned {}", enable ? "on" : "off");
	if (auto* gameMusic = ScriptGameMusic())
	{
		gameMusic->SetAlignmentMusicEnabled(enable);
	}
}

void AttachSoundTag() // 447 ATTACH_SOUND_TAG
{
	// An object keeps up a sound from a script bank: it plays from the object while the camera is in its range
	const auto object = PopObject();
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	const auto threeD = Pop().intVal != 0;
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		return;
	}
	if (!threeD)
	{
		// A sound kept up by an object but not heard from it. No script asks for one.
		// TODO(raffclar): sound tags that aren't heard from their object
		NotImplemented();
		return;
	}
	const auto sound = BankSound(bank, sample);
	if (!sound.has_value() || !Locator::soundTagSystem::has_value())
	{
		return;
	}
	// A script's tag plays its sound one time each start, and the next turn starts it again when it is finished. It
	// adds to the sounds that the object keeps up already.
	Locator::soundTagSystem::value().AttachObjectSound(object, *sound, false);
}

void DetachSoundTag() // 448 DETACH_SOUND_TAG
{
	const auto object = PopObject();
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	if (object == entt::null || !Locator::entitiesRegistry::value().Valid(object))
	{
		return;
	}
	if (const auto sound = BankSound(bank, sample); sound.has_value() && Locator::soundTagSystem::has_value())
	{
		Locator::soundTagSystem::value().DetachObjectSound(object, *sound);
	}
}

void GameSoundPlaying() // 450 GAME_SOUND_PLAYING
{
	// Whether a sample that a script started from a script bank still plays. The sample number is the owner.
	const auto bank = Pop().intVal;
	const auto sample = Pop().intVal;
	const auto sound = BankSound(bank, sample);
	Pushb(sound.has_value() && Locator::sounds::value().IsNumberedSoundEffectPlaying(*sound, static_cast<uint32_t>(sample)));
}

void SaySoundEffectPlaying() // 458 SAY_SOUND_EFFECT_PLAYING
{
	const auto text = static_cast<uint32_t>(Pop().intVal);
	const auto extra = Pop().intVal != 0;
	Pushb(Locator::helpSpeechSystem::value().IsSaying(text, extra ? audio::SpeechVoice::Second : audio::SpeechVoice::First));
}

} // namespace openblack::chlapi
