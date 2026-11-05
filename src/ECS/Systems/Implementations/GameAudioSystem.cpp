/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameAudioSystem.h"

#include <optional>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/TempleInteriorInterface.h"
#include "Audio/AtmosAudio.h"
#include "Camera/Camera.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Weather.h"
#include "ECS/CreatureDanceMusic.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ScriptMusicSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/VideoSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "ECS/WorshipChantMusic.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Sound/GameMusic.h"
#include "Sound/GameMusicTurn.h"

namespace openblack::ecs::systems
{

GameAudioSystem::GameAudioSystem() = default;

GameAudioSystem::~GameAudioSystem() = default;

void GameAudioSystem::StartLandAudio(bool loadMusic)
{
	if (!_atmosAudio)
	{
		_atmosAudio = std::make_unique<audio::AtmosAudio>();
	}
	_atmosAudio->Init();
	if (!_gameMusic)
	{
		// The -NOLOADMUSIC switch: the game loads none of its music banks, so none of its music plays
		_gameMusic = std::make_unique<audio::GameMusic>(loadMusic);
		// The script, the saves and the films find the music through the locator. This system stays the owner.
		Locator::gameMusic::reset(_gameMusic.get(), [](audio::GameMusic* /*unowned*/) {});
		if (Locator::scriptMusicSystem::has_value())
		{
			Locator::scriptMusicSystem::value().SetBanksLoaded(loadMusic);
			Locator::scriptMusicSystem::value().RegisterTimings();
		}
		if (loadMusic)
		{
			audio::GameMusic::RegisterBanks();
		}
	}
	_gameMusic->Reset();
}

void GameAudioSystem::ProcessMusicTurn(glm::vec3 camera, bool inCitadel)
{
	// The game picks the music for the turn
	if (!_gameMusic)
	{
		return;
	}
	const auto& alignment = Locator::alignmentSystem::value();
	bool cinema = false;
	if (Locator::cinematicDirectorSystem::has_value())
	{
		const auto& director = Locator::cinematicDirectorSystem::value();
		cinema = audio::game_music_turn::CinemaHoldsLandMusic({
		    .on = director.IsWideScreenOn(),
		    .owner = director.GetWideScreenOwner(),
		    .transitionFinished = director.IsWideScreenTransitionFinished(),
		});
	}
	auto music = audio::game_music_turn::InputsAt({
	    .turn = Locator::time::value().GetTurn(),
	    .camera = camera,
	    .groundHeight = Locator::terrainSystem::value().GetHeightAt(glm::xz(camera)),
	    .inCitadel = inCitadel,
	    .alignment = alignment.GetCameraAlignment(),
	    .playerAlignment = alignment.GetPlayerAlignment(
	        (Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE)),
	    .cinema = cinema,
	});
	music.thingPosition = [](uint32_t thing) -> std::optional<glm::vec3> {
		const auto& registry = Locator::entitiesRegistry::value();
		const auto entity = static_cast<entt::entity>(thing);
		if (!registry.Valid(entity) || !registry.AllOf<components::Transform>(entity))
		{
			return std::nullopt;
		}
		return registry.Get<const components::Transform>(entity).position;
	};
	music.worshipChant = worship_chant_music::Heard(camera);
	music.creatureDance = creature_dance_music::Heard(camera);
	// The music heard follows the world's clock of the scripts' music
	if (Locator::scriptMusicSystem::has_value())
	{
		const auto& script = Locator::scriptMusicSystem::value().GetMusic();
		music.script = {
		    .type = script.GetType(),
		    .started = script.IsStarted(),
		    .playCount = script.GetPlayCount(),
		    .place = script.GetPlace(),
		};
	}
	music.film = Locator::videoSystem::has_value() && Locator::videoSystem::value().IsPlaying();
	if (Locator::infoConstants::has_value())
	{
		const auto& sound = Locator::infoConstants::value().sound;
		music.townTriggerDistance = sound.townTriggerDistance;
		music.townTriggerOffDistance = sound.townTriggerOffDistance;
	}
	if (Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().FightMusicHeard(camera))
	{
		const auto& context = Locator::entitiesRegistry::value().Context();
		music.creatureFight = audio::GameMusic::SelectFightType(context.mapScriptGlobals.landNumber, context.skirmish);
	}
	Locator::entitiesRegistry::value().Each<const components::Town, const Tribe, const components::Transform>(
	    [&music](const components::Town& town, const Tribe tribe, const components::Transform& transform) {
		    music.towns.push_back({
		        .position = transform.position,
		        .tribe = static_cast<int32_t>(tribe),
		        .id = town.id,
		    });
	    });
	_gameMusic->ProcessTurn(music);
}

void GameAudioSystem::ProcessAmbienceTurn(glm::vec3 camera, bool inCitadel)
{
	// The ambience follows the weather at the camera
	if (!_atmosAudio)
	{
		return;
	}
	components::WeatherInfo weather {};
	if (Locator::weatherSystem::has_value())
	{
		weather = Locator::weatherSystem::value().GetWeatherSmooth(camera);
	}
	_atmosAudio->SetAlignment(Locator::alignmentSystem::value().GetCameraAlignment());
	_atmosAudio->EndTurn({
	    .camera = camera,
	    .weather =
	        {
	            .rain = weather.rain,
	            .snow = weather.snow,
	            .windX = weather.windX,
	            .windZ = weather.windZ,
	        },
	    .paused = false,
	    .turn = Locator::time::value().GetTurn(),
	    .inCitadel = inCitadel,
	    .videoPlaying = Locator::videoSystem::value().IsPlaying(),
	});
}

void GameAudioSystem::ProcessTempleAudioTurn()
{
	// The temple's turns play the citadel's music, while the land's ambience fades out
	ProcessMusicTurn(Locator::camera::value().GetOrigin(), true);
	if (_atmosAudio)
	{
		_atmosAudio->ContinueTurn({.paused = false,
		                           .turn = Locator::time::value().GetTurn(),
		                           .inCitadel = true,
		                           .videoPlaying = Locator::videoSystem::value().IsPlaying()});
	}
}

} // namespace openblack::ecs::systems
