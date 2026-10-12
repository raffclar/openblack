/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CloudSystem.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "3D/Clouds.h"
#include "3D/LandIslandInterface.h"
#include "3D/Lightning.h"
#include "3D/StormClouds.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/CloudArchetype.h"
#include "ECS/Archetypes/StormCloudArchetype.h"
#include "ECS/Components/Cloud.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/StormCloud.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Weather.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

void CloudSystem::Reset()
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> old;
	registry.Each<const Cloud>([&old](entt::entity entity, const Cloud& /*unused*/) { old.push_back(entity); });
	for (const auto entity : old)
	{
		registry.Destroy(entity);
	}
	// The game lays them out from the C runtime's numbers
	auto& random = Locator::gameRandom::value();
	for (const auto& layout : clouds::MakeLayout([&random](float low, float high) { return random.CrtRandom(low, high); }))
	{
		archetypes::CloudArchetype::Create(layout);
	}
}

void CloudSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	Locator::entitiesRegistry::value().Each<Cloud, Mist, Transform>([gameTime](Cloud& cloud, Mist& mist, Transform& transform) {
		cloud.track = clouds::Move(cloud.track, cloud.pinned, gameTime.count());
		transform.position = clouds::WorldPosition(cloud.track);
		mist.colour = static_cast<uint32_t>(clouds::EdgeAlpha(cloud.track, cloud.pinned)) << 24u;
	});
}

void CloudSystem::UpdateStormClouds(std::chrono::milliseconds frameRealTime)
{
	auto& registry = Locator::entitiesRegistry::value();

	// Each storm with its clouds in the order they gathered, the newest storm first as the game goes through them
	struct Gathering
	{
		entt::entity storm;
		uint32_t serial;
		std::vector<entt::entity> clouds;
	};
	std::vector<Gathering> storms;
	registry.Each<const Storm>([&storms](entt::entity entity, const Storm& storm) {
		storms.push_back({.storm = entity, .serial = storm.serial, .clouds = {}});
	});
	std::ranges::sort(storms, [](const Gathering& a, const Gathering& b) { return a.serial > b.serial; });

	// The clouds of storms that have gone go with them
	std::vector<entt::entity> gone;
	registry.Each<const StormCloud>([&storms, &gone](entt::entity entity, const StormCloud& cloud) {
		const auto owner = std::ranges::find(storms, cloud.storm, &Gathering::storm);
		if (owner == storms.end())
		{
			gone.push_back(entity);
			return;
		}
		owner->clouds.push_back(entity);
	});
	for (const auto entity : gone)
	{
		registry.Destroy(entity);
	}

	if (!storm_clouds::StepDue(_sinceStormStep, frameRealTime))
	{
		return;
	}

	// The game draws their numbers from the C runtime's
	auto& gameRandom = Locator::gameRandom::value();
	const storm_clouds::Random random = [&gameRandom](float low, float high) { return gameRandom.CrtRandom(low, high); };
	const auto* terrain = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;

	for (auto& gathering : storms)
	{
		const auto& storm = registry.Get<const Storm>(gathering.storm);
		std::ranges::sort(gathering.clouds, [&registry](entt::entity a, entt::entity b) {
			return registry.Get<const StormCloud>(a).index < registry.Get<const StormCloud>(b).index;
		});
		const auto wanted = storm_clouds::Wanted(storm.cloudCount);
		if (wanted == 0)
		{
			// A storm told to have no clouds shows none of those it gathered, and they keep still
			for (const auto entity : gathering.clouds)
			{
				registry.Get<Mist>(entity).colour = 0;
			}
			continue;
		}
		// One more gathers each step until it has its count
		if (std::cmp_less(gathering.clouds.size(), wanted))
		{
			gathering.clouds.push_back(archetypes::StormCloudArchetype::Create(
			    gathering.storm, static_cast<uint8_t>(gathering.clouds.size()), storm.cloudHeight, random));
		}

		const auto alpha = storm_clouds::Alpha(storm.currentStrength);
		const auto shown = alpha > storm_clouds::k_LeastShownAlpha;
		const auto flashGrey = storm_clouds::FlashGrey(lightning::GlowStrength(storm.flash));
		for (const auto entity : gathering.clouds)
		{
			auto& cloud = registry.Get<StormCloud>(entity);
			storm_clouds::Step(cloud.drift, random, storm.cloudHeight);
			const auto place = storm_clouds::Place(cloud.drift, cloud.sizeFactor, storm.currentPosition,
			                                       storm.currentInnerRadius, storm.outerRadius);
			const float ground = terrain != nullptr ? terrain->GetHeightAt(place.xz) : 0.0f;
			registry.Get<Transform>(entity).position = {place.xz.x, ground + place.heightAboveGround, place.xz.y};
			auto& mist = registry.Get<Mist>(entity);
			mist.size = place.size;
			mist.colour = shown ? static_cast<uint32_t>(alpha) << 24u : 0u;
			cloud.shade = storm.cloudShade;
			cloud.flashGrey = flashGrey;
		}
	}
}
