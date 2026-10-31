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
#include "3D/Mists.h"
#include "3D/StormClouds.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Cloud.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Weather.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;

void CloudSystem::Reset()
{
	// The storms of the last land took their clouds with them
	_stormClouds.clear();
}

void CloudSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	Locator::drawnRegistry::value().Each<Cloud, Mist, Transform>([gameTime](Cloud& cloud, Mist& mist, Transform& transform) {
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
		std::vector<StormCloud*> clouds;
	};
	std::vector<Gathering> storms;
	registry.Each<const Storm>([&storms](entt::entity entity, const Storm& storm) {
		storms.push_back({.storm = entity, .serial = storm.serial, .clouds = {}});
	});
	std::ranges::sort(storms, [](const Gathering& a, const Gathering& b) { return a.serial > b.serial; });

	// The clouds of storms that have gone go with them
	std::erase_if(_stormClouds, [&storms](const StormCloud& cloud) {
		return std::ranges::find(storms, cloud.storm, &Gathering::storm) == storms.end();
	});

	if (!storm_clouds::StepDue(_sinceStormStep, frameRealTime))
	{
		return;
	}

	// Each storm gathers at most one cloud in a step. The room for them now keeps the pointers into the list good.
	_stormClouds.reserve(_stormClouds.size() + storms.size());
	for (auto& cloud : _stormClouds)
	{
		std::ranges::find(storms, cloud.storm, &Gathering::storm)->clouds.push_back(&cloud);
	}

	// The game draws their numbers from the C runtime's
	auto& gameRandom = Locator::gameRandom::value();
	const storm_clouds::Random random = [&gameRandom](float low, float high) { return gameRandom.CrtRandom(low, high); };
	const auto* terrain = Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;

	for (auto& gathering : storms)
	{
		const auto& storm = registry.Get<const Storm>(gathering.storm);
		std::ranges::sort(gathering.clouds, {}, &StormCloud::index);
		const auto wanted = storm_clouds::Wanted(storm.cloudCount);
		if (wanted == 0)
		{
			// A storm told to have no clouds shows none of those it gathered, and they keep still
			for (auto* cloud : gathering.clouds)
			{
				cloud->mist.colour = 0;
			}
			continue;
		}
		// One more gathers each step until it has its count
		if (std::cmp_less(gathering.clouds.size(), wanted))
		{
			// The animation of the mist takes its number first, then the cloud its shape and place
			const auto counter = mists::StartCounter(random(0.0f, 16.0f));
			const auto shape = storm_clouds::MakeCloud(random, storm.cloudHeight);
			// The first step places and colours it, and until then it is not seen
			gathering.clouds.push_back(&_stormClouds.emplace_back(StormCloud {
			    .storm = gathering.storm,
			    .index = static_cast<uint8_t>(gathering.clouds.size()),
			    .drift = shape.drift,
			    .sizeFactor = shape.sizeFactor,
			    .position = glm::vec3(0.0f),
			    .mist =
			        Mist {
			            .size = 1.0f,
			            .colour = 0,
			            .shrinksEdgeOn = true,
			            .edgeShrink = shape.edgeShrink,
			            .counter = counter,
			            .counterRemainder = 0.0f,
			        },
			    .shade = 0.0f,
			    .flashGrey = 0,
			}));
		}

		const auto alpha = storm_clouds::Alpha(storm.currentStrength);
		const auto shown = alpha > storm_clouds::k_LeastShownAlpha;
		const auto flashGrey = storm_clouds::FlashGrey(lightning::GlowStrength(storm.flash));
		for (auto* cloud : gathering.clouds)
		{
			storm_clouds::Step(cloud->drift, random, storm.cloudHeight);
			const auto place = storm_clouds::Place(cloud->drift, cloud->sizeFactor, storm.currentPosition,
			                                       storm.currentInnerRadius, storm.outerRadius);
			const float ground = terrain != nullptr ? terrain->GetHeightAt(place.xz) : 0.0f;
			cloud->position = {place.xz.x, ground + place.heightAboveGround, place.xz.y};
			cloud->mist.size = place.size;
			cloud->mist.colour = shown ? static_cast<uint32_t>(alpha) << 24u : 0u;
			cloud->shade = storm.cloudShade;
			cloud->flashGrey = flashGrey;
		}
	}
}

void CloudSystem::AnimateStormClouds(std::chrono::duration<float, std::milli> gameTime)
{
	for (auto& cloud : _stormClouds)
	{
		mists::Advance(cloud.mist.counter, cloud.mist.counterRemainder, gameTime.count());
	}
}
