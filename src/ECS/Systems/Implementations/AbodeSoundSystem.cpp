/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "AbodeSoundSystem.h"

#include <string>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/TempleInteriorInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "Common/X87Vector.h"
#include "ECS/AbodeSounds.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/ObjectDrawListSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;

void AbodeSoundSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	if (!Locator::audio::has_value() || !Locator::objectDrawListSystem::has_value() || !Locator::skySystem::has_value() ||
	    !Locator::camera::has_value() || !Locator::gameRandom::has_value())
	{
		return;
	}
	// The temple hides the land, so the game draws no building there
	if (Locator::temple::has_value() && Locator::temple::value().Active())
	{
		return;
	}
	const auto& drawList = Locator::objectDrawListSystem::value();
	if (!drawList.GetView().has_value())
	{
		return;
	}
	// A roll from 0 to 0 never gives 1, so no building sounds
	const auto chance = abode_sounds::Chance(gameTime);
	if (chance <= 1)
	{
		return;
	}

	const auto& registry = Locator::entitiesRegistry::value();
	const auto& sky = Locator::skySystem::value();
	const auto hour = sky.GetTime();
	const auto keys = abode_sounds::KeysAt(abode_sounds::Darkness(hour, sky.GetDayNightTimes()), hour).ToArray();
	const auto camera = Locator::camera::value().GetOrigin();
	auto& random = Locator::gameRandom::value();
	auto& audio = Locator::audio::value();
	const auto bank = std::string(abode_sounds::k_Bank);

	for (const auto entity : drawList.GetList())
	{
		if (!registry.AllOf<components::Abode, components::Transform>(entity) || !drawList.Drawn(registry, entity))
		{
			continue;
		}
		const auto* info = world_objects::AbodeInfoOf(entity);
		if (info == nullptr || !abode_sounds::MakesSounds(info->abodeType))
		{
			continue;
		}
		// Each drawn building rolls its chance, then the camera must be near
		if (!abode_sounds::RollSounds(random.LocalRand(static_cast<int32_t>(chance))))
		{
			continue;
		}
		const auto& position = registry.Get<components::Transform>(entity).position;
		if (!abode_sounds::InReach(x87::Distance<x87::SumOrder::Zyx>(position, camera)))
		{
			continue;
		}
		// The sound plays only as its sample allows, as the scripts' sound effects do
		const auto played = audio.PlayAnimEffect(bank, keys, entity, position, audio::CurrentSoundEffectConditions());
		// The sound stays with its building, and it stops when the building goes
		audio.SetEmitterFollows(played.emitter, audio::Follows::Owner);
		++_sounds;
		SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Building {} makes its sound {}: {}/{} ({})", entt::to_integral(entity),
		                    keys[4], bank, played.sample.value_or(-1), static_cast<int>(played.outcome));
	}
}
