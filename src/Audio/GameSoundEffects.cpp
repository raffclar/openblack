/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameSoundEffects.h"

#include <algorithm>

#include <spdlog/spdlog.h>

#include "3D/TempleInteriorInterface.h"
#include "CHLApi.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::audio
{

namespace
{
/// The bank of the loaded sound group a sound is in, None when it is in none
ScriptSoundBank BankOfSound(entt::id_type sound)
{
	if (!Locator::audio::has_value())
	{
		return ScriptSoundBank::None;
	}
	for (const auto& [name, group] : Locator::audio::value().GetSoundGroups())
	{
		if (std::ranges::find(group.sounds, sound) != group.sounds.end())
		{
			return BankOfFile(name);
		}
	}
	return ScriptSoundBank::None;
}
} // namespace

SoundEffectConditions CurrentSoundEffectConditions()
{
	SoundEffectConditions conditions;
	if (Locator::cinematicDirectorSystem::has_value())
	{
		const auto& director = Locator::cinematicDirectorSystem::value();
		conditions.scriptWideScreen = director.IsWideScreenOn() && director.GetWideScreenOwner() != 0;
	}
	conditions.insideTemple = Locator::temple::has_value() && Locator::temple::value().Active();
	conditions.gameSoundOn = !Locator::chlapi::has_value() || Locator::chlapi::value().IsGameSoundOn();
	conditions.creatureFightControl =
	    Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().DirectsFight();
	return conditions;
}

bool GameSoundEffectHeard(const SoundEffectConditions& conditions, entt::id_type sound)
{
	if (!Locator::resources::has_value())
	{
		return false;
	}
	const auto& sounds = Locator::resources::value().GetSounds();
	if (!sounds.Contains(sound))
	{
		return false;
	}
	// The bank only matters while the scripts have the game's sound off
	const auto bank = conditions.gameSoundOn ? ScriptSoundBank::InGame : BankOfSound(sound);
	const auto& handle = sounds.Handle(sound);
	if (SoundEffectHeard(conditions, bank, handle->userParam))
	{
		return true;
	}
	// The log may not be set up (tests)
	if (const auto logger = spdlog::get("audio"); logger != nullptr)
	{
		SPDLOG_LOGGER_DEBUG(logger, "Sound {} not heard (use {}): {}{}{}{}", handle->name, handle->userParam,
		                    conditions.scriptWideScreen ? "cut scene " : "", conditions.insideTemple ? "in the temple " : "",
		                    conditions.gameSoundOn ? "" : "game sound off ",
		                    conditions.creatureFightControl ? "fight controls" : "");
	}
	return false;
}

void PlayGameSoundEffect(entt::id_type sound, std::optional<glm::vec3> worldPosition)
{
	if (Locator::audio::has_value() && GameSoundEffectHeard(CurrentSoundEffectConditions(), sound))
	{
		Locator::audio::value().PlaySoundEffect(sound, worldPosition);
	}
}

entt::entity StartGameSoundEffect(entt::id_type sound, const SoundEffectOptions& options)
{
	if (!Locator::audio::has_value() || !GameSoundEffectHeard(CurrentSoundEffectConditions(), sound))
	{
		return entt::null;
	}
	return Locator::audio::value().StartSoundEffect(sound, options);
}

} // namespace openblack::audio
