/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameSoundEffects.h"

#include "3D/TempleInteriorInterface.h"
#include "CHLApi.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "Locator.h"

namespace openblack::audio
{

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
	return Locator::sounds::has_value() && Locator::sounds::value().IsGameEffectHeard(conditions, sound);
}

void PlayGameSoundEffect(entt::id_type sound, std::optional<glm::vec3> worldPosition)
{
	if (Locator::sounds::has_value())
	{
		Locator::sounds::value().PlayGameEffect(sound, worldPosition);
	}
}

entt::entity StartGameSoundEffect(entt::id_type sound, const SoundEffectOptions& options)
{
	if (!Locator::sounds::has_value())
	{
		return entt::null;
	}
	return Locator::sounds::value().StartGameEffect(sound, options);
}

} // namespace openblack::audio
