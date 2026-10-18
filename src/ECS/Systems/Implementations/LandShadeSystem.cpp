/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LandShadeSystem.h"

#include <algorithm>
#include <utility>

#include <LNDFile.h>
#include <entt/core/hashed_string.hpp>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "3D/StormClouds.h"
#include "3D/VillageLights.h"
#include "ECS/Components/Cloud.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/Components/Weather.h"
#include "ECS/Registry.h"
#include "Graphics/HandLight.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Scenery/ObjectDrawList.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The shadow images of the sky's clouds and of the storms
constexpr entt::hashed_string k_CloudShadowId = entt::hashed_string("raw/sclouds");
constexpr entt::hashed_string k_StormShadowId = entt::hashed_string("raw/sstorm");
/// A land block is 160 units a side
constexpr float k_BlockSize = 160.0f;

LandShadeSystem::Sources GameSources()
{
	return {
	    .image = [](entt::id_type id) -> std::span<const uint8_t> {
		    if (!Locator::resources::has_value())
		    {
			    return {};
		    }
		    const auto& textures = Locator::resources::value().GetTextures();
		    if (!textures.Contains(id))
		    {
			    return {};
		    }
		    return textures.Handle(id)->GetTexels();
	    },
	    .landIdentity = []() -> std::optional<uint64_t> {
		    if (!Locator::terrainSystem::has_value())
		    {
			    return std::nullopt;
		    }
		    const auto& island = Locator::terrainSystem::value();
		    if (!island.IsLoaded() || island.GetBlocks().empty())
		    {
			    return std::nullopt;
		    }
		    // A new land makes its luminosity map again
		    const auto handle = island.GetLuminosityMap().GetNativeHandle();
		    return (static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&island.GetLuminosityMap())) << 16u) ^
		           static_cast<uint64_t>(handle.id);
	    },
	    .lay =
	        []() {
		        LandShadeSystem::LaidLand land;
		        const auto& island = Locator::terrainSystem::value();
		        for (const auto& block : island.GetBlocks())
		        {
			        const auto& lnd = block.GetLndBlock();
			        if (lnd == nullptr)
			        {
				        continue;
			        }
			        land_shade::LaidBlock laid {.at = block.GetBlockPosition(), .luminosity = {}};
			        std::ranges::transform(lnd->cells, laid.luminosity.begin(),
			                               [](const lnd::LNDCell& cell) { return cell.luminosity; });
			        land.blocks.push_back(laid);
			        land.highest.push_back(std::ranges::max(lnd->cells, {}, &lnd::LNDCell::altitude).altitude);
		        }
		        land.firstBlock = glm::ivec2(island.GetIndexExtent().minimum);
		        land.mapSize = island.GetLuminosityMap().GetResolution();
		        return land;
	        },
	    .registry = []() -> ecs::Registry* {
		    return Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
	    },
	};
}
} // namespace

LandShadeSystem::LandShadeSystem()
    : LandShadeSystem(GameSources())
{
}

LandShadeSystem::LandShadeSystem(Sources sources)
    : _sources(std::move(sources))
{
}

void LandShadeSystem::Clear()
{
	_identity.reset();
	_land.Lay({});
	_highest.clear();
	_map.clear();
	_mapSize = {0, 0};
	_firstChangedRow = _lastChangedRow = 0;
	_shadows = _lights = 0;
}

void LandShadeSystem::Lay()
{
	auto laid = _sources.lay();
	_land.Lay(laid.blocks);
	_highest = std::move(laid.highest);
	_firstBlock = laid.firstBlock;
	_mapSize = laid.mapSize;
	_map.assign(static_cast<size_t>(_mapSize.x) * _mapSize.y, 0xFF);
	// The whole map is new, its blocks in the order they were laid
	const auto blocks = _land.Blocks();
	UpdateMap(blocks);
	_firstChangedRow = 0;
	_lastChangedRow = _mapSize.y;
}

void LandShadeSystem::Shade(const LandShadeFrame& frame)
{
	const auto identity = _sources.landIdentity();
	auto* registry = _sources.registry();
	if (!identity.has_value() || registry == nullptr)
	{
		return;
	}
	const bool laidNow = identity != _identity;
	if (laidNow)
	{
		_identity = identity;
		Lay();
	}

	// What the last frame changed goes back as it was laid
	const std::vector<glm::ivec2> changedBefore(_land.Changed().begin(), _land.Changed().end());
	_land.Restore();

	// The blocks in view take the shadows
	const auto blocks = _land.Blocks();
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		const object_draw_list::Block box {
		    .corner = glm::vec2(blocks[i]) * k_BlockSize,
		    .highestAltitude = i < _highest.size() ? _highest[i] : uint8_t {0},
		    .reflected = frame.landReflection,
		};
		_land.SetInView(blocks[i], object_draw_list::BlockInView(frame.viewProjection, frame.nearPlane, box));
	}

	CastShadows(frame, *registry);
	CastLights(frame, *registry);

	if (laidNow)
	{
		UpdateMap(_land.Changed());
		_firstChangedRow = 0;
		_lastChangedRow = _mapSize.y;
		return;
	}
	std::vector<glm::ivec2> both = changedBefore;
	both.insert(both.end(), _land.Changed().begin(), _land.Changed().end());
	UpdateMap(both);
}

void LandShadeSystem::CastShadows(const LandShadeFrame& frame, Registry& registry)
{
	_shadows = 0;
	size_t room = land_shade::k_MostStamps;
	const auto cast = [this, &room](const land_shade::Shadow& shadow) {
		if (room == 0)
		{
			return;
		}
		--room;
		++_shadows;
		land_shade::CastShadow(_land, shadow);
	};

	// The particles' stamps come first: their shadows darken the land, their colours only take places
	for (const auto& stamp : frame.particleStamps)
	{
		if (stamp.has_value())
		{
			cast(*stamp);
		}
		else if (room > 0)
		{
			--room;
		}
	}
	// Then the flashes of lightning, which also only take places
	room -= std::min(frame.flashes, room);

	if (!frame.cloudShadows)
	{
		return;
	}

	// The sky's clouds in the order they were made: the registry walks the newest first
	const auto cloudImage = _sources.image(k_CloudShadowId.value());
	struct CloudShade
	{
		glm::vec2 corner;
		uint32_t alpha;
	};
	std::vector<CloudShade> clouds;
	registry.Each<const ecs::components::Cloud, const ecs::components::Mist, const ecs::components::Transform>(
	    [&clouds, &frame](const ecs::components::Cloud& /*unused*/, const ecs::components::Mist& mist,
	                      const ecs::components::Transform& transform) {
		    clouds.push_back({.corner = {transform.position.x, transform.position.z},
		                      .alpha = ((mist.colour >> 24u) * frame.skyAlpha) / 255u});
	    });
	std::ranges::reverse(clouds);
	for (const auto& cloud : clouds)
	{
		if (cloud.alpha == 0)
		{
			continue;
		}
		cast({.corner = cloud.corner,
		      .image = cloudImage,
		      .side = land_shade::k_ShadowSide,
		      .alpha = land_shade::Alpha(land_shade::CloudStrength(cloud.alpha))});
	}

	// The storms with clouds of their own, the newest first, each shadow centred on its storm
	const auto stormImage = _sources.image(k_StormShadowId.value());
	std::vector<const ecs::components::Storm*> storms;
	registry.Each<const ecs::components::Storm>([&storms](const ecs::components::Storm& storm) { storms.push_back(&storm); });
	std::ranges::sort(storms, [](const auto* a, const auto* b) { return a->serial > b->serial; });
	for (const auto* storm : storms)
	{
		if (storm_clouds::Wanted(storm->cloudCount) == 0)
		{
			continue;
		}
		const auto alpha = storm_clouds::ShadowAlpha(storm->cloudShade, storm->currentStrength);
		if (!alpha.has_value())
		{
			continue;
		}
		cast({.corner = land_shade::Centred({storm->currentPosition.x, storm->currentPosition.z}, land_shade::k_ShadowSide),
		      .image = stormImage,
		      .side = land_shade::k_ShadowSide,
		      .alpha = *alpha});
	}
}

void LandShadeSystem::CastLights(const LandShadeFrame& frame, Registry& registry)
{
	_lights = 0;
	if (!village_lights::IsDark(frame.landColour))
	{
		return;
	}
	// The hand's light first, from the corner of its image
	if (frame.hand.has_value())
	{
		const auto image = WarmImage(village_lights::k_HandImageId.value());
		land_shade::CastLight(_land,
		                      {.corner = graphics::HandLight::GetOrigin(*frame.hand),
		                       .image = image,
		                       .side = graphics::HandLight::k_Size,
		                       .strength = graphics::HandLight::GetStrength(frame.landColour)},
		                      frame.fullLightGreen);
		++_lights;
	}
	// Then the villages' lights, the newest first, each from a little before it where it has flickered to
	const auto image = WarmImage(village_lights::k_VillageImageId.value());
	const auto side = static_cast<int32_t>(village_lights::k_VillageImageSide);
	const float strength = village_lights::VillageBrightness(village_lights::Intensity(frame.scriptHour));
	registry.Each<const ecs::components::VillageLight, const ecs::components::Transform>(
	    [&](const ecs::components::VillageLight& light, const ecs::components::Transform& transform) {
		    const glm::vec2 corner {(transform.position.x - village_lights::k_VillageReach) + light.flicker.x,
		                            (transform.position.z - village_lights::k_VillageReach) + light.flicker.y};
		    land_shade::CastLight(_land, {.corner = corner, .image = image, .side = side, .strength = strength},
		                          frame.fullLightGreen);
		    ++_lights;
	    });
}

std::span<const uint8_t> LandShadeSystem::WarmImage(entt::id_type id)
{
	if (const auto found = _warmImages.find(id); found != _warmImages.end() && !found->second.empty())
	{
		return found->second;
	}
	const auto raw = _sources.image(id);
	if (raw.empty())
	{
		return {};
	}
	return _warmImages[id] = land_shade::WarmImage(raw);
}

void LandShadeSystem::UpdateMap(std::span<const glm::ivec2> changed)
{
	_firstChangedRow = _lastChangedRow = 0;
	if (_map.empty() || changed.empty())
	{
		return;
	}
	// The blocks out of view first, so that a cell shared with a block in view shows that block's copy
	auto first = static_cast<int32_t>(_mapSize.y);
	int32_t last = 0;
	for (const bool inView : {false, true})
	{
		for (const auto block : changed)
		{
			if (_land.InView(block) != inView)
			{
				continue;
			}
			const auto* cells = _land.Find(block);
			if (cells == nullptr)
			{
				continue;
			}
			const glm::ivec2 origin = (block - _firstBlock) * land_shade::k_BlockStep;
			for (int32_t x = 0; x < land_shade::k_BlockSide; ++x)
			{
				for (int32_t z = 0; z < land_shade::k_BlockSide; ++z)
				{
					const auto u = origin.x + x;
					const auto v = origin.y + z;
					if (u < 0 || v < 0 || u >= _mapSize.x || v >= _mapSize.y)
					{
						continue;
					}
					_map[(static_cast<size_t>(v) * _mapSize.x) + static_cast<size_t>(u)] =
					    (*cells)[static_cast<size_t>((x * land_shade::k_BlockSide) + z)];
				}
			}
			first = std::min(first, std::max(origin.y, 0));
			last = std::max(last, std::min(origin.y + land_shade::k_BlockSide, static_cast<int32_t>(_mapSize.y)));
		}
	}
	if (first < last)
	{
		_firstChangedRow = static_cast<uint16_t>(first);
		_lastChangedRow = static_cast<uint16_t>(last);
	}
}

LandShadeMap LandShadeSystem::Map() const
{
	return {.texels = _map, .size = _mapSize, .firstChangedRow = _firstChangedRow, .lastChangedRow = _lastChangedRow};
}
